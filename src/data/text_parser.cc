#include "data/text_parser.h"
#include <functional>
#include "util/strtonum.h"
#include "util/resource_usage.h"
#include "util/MurmurHash3.h"
#include "base/matrix_io_inl.h"

namespace PS {

DECLARE_bool(shuffle_fea_id);

// NOTICE: Do not use strtok, it is not thread-safe, use strtok_r instead
TextParser::TextParser(TextFormat format, bool ignore_feature_group) {
  ignore_fea_grp_ = ignore_feature_group;
  using namespace std::placeholders;
  switch (format) {
    case DataConfig::LIBSVM:
      convertor_ = std::bind(&TextParser::parseLibsvm, this, _1, _2);
      info_.set_fea_type(InstanceInfo::SPARSE);
      info_.set_label_type(InstanceInfo::BINARY);
      break;
    case DataConfig::ADFEA:
      convertor_ = std::bind(&TextParser::parseAdfea, this, _1, _2);
      info_.set_fea_type(InstanceInfo::SPARSE_BINARY);
      info_.set_label_type(InstanceInfo::BINARY);
      break;
    case DataConfig::TERAFEA:
      convertor_ = std::bind(&TextParser::parseTerafea, this, _1, _2);
      info_.set_fea_type(InstanceInfo::SPARSE_BINARY);
      info_.set_label_type(InstanceInfo::BINARY);
      break;
    default:
      CHECK(false) << "unknown text format " << format;
  }
}

bool TextParser::toProto(char* line, Instance* ins) {
  // convert to protobuf format
  ins->Clear(); if (!convertor_(line, ins)) return false;
  // update info
  for (int i = 0; i < ins->fea_grp_size(); ++i) {
    int grp_id = ins->fea_grp(i).grp_id();
    if (grp_id >= kGrpIDmax) return false;
    auto& info = grp_info_[grp_id];
    for (int j = 0; j < ins->fea_grp(i).fea_id_size(); ++j) {
      uint64 fea_id = ins->fea_grp(i).fea_id(j);
      info.set_fea_begin(std::min((uint64)info.fea_begin(), fea_id));
      info.set_fea_end(std::max((uint64)info.fea_end(), fea_id + 1));
    }
    info.set_nnz_ins(info.nnz_ins() + 1);
    info.set_nnz_ele(info.nnz_ele() + ins->fea_grp(i).fea_id_size());
  }
  if (ins->has_label()) {
    auto& lbl = grp_info_[kGrpIDmax];
    lbl.set_nnz_ins(lbl.nnz_ins() + 1);
  }
  ++ num_ins_;
  return true;
}

InstanceInfo TextParser::info() {
  info_.clear_fea_grp();
  info_.set_num_ins(num_ins_);
  if (info_.label_type() != InstanceInfo::EMPTY) {
    auto& lbl = grp_info_[kGrpIDmax];
    lbl.set_nnz_ele(lbl.nnz_ins());
    lbl.set_fea_begin(0);
    lbl.set_fea_end(1);
  }
  for (int i = 0; i <= kGrpIDmax; ++i) {
    if (grp_info_[i].nnz_ele() > 0) {
      grp_info_[i].set_grp_id(i);
      *info_.add_fea_grp() = grp_info_[i];
      if (i == kGrpIDmax) continue;  // skip label
      info_.set_fea_begin(std::min(info_.fea_begin(), grp_info_[i].fea_begin()));
      info_.set_fea_end(std::max(info_.fea_end(), grp_info_[i].fea_end()));
      info_.set_nnz_ele(info_.nnz_ele() + grp_info_[i].nnz_ele());
    }
  }
  return info_;
}

// libsvm:
//
//   label feature_id:weight feature_id:weight feature_id:weight ...
//
// assume feature_ids are ordered

bool TextParser::parseLibsvm(char* buff, Instance* ins) {

  char *saveptr;
  char * pch = strtok_r(buff, " \t\r\n", &saveptr);
  uint64 idx, last_idx=0;
  float label, val;

  if (!strtofloat(pch, &label)) return false;
  ins->set_label(label);
  if (floor(label) != label) {
    info_.set_label_type(InstanceInfo::CONTINUOUS);
  } else if (label != 1 && label != -1) {
    info_.set_label_type(InstanceInfo::MULTICLASS);
  }
  pch = strtok_r(NULL, " \t\r\n", &saveptr);

  auto grp = ins->add_fea_grp();
  grp->set_grp_id(1);
  while (pch != NULL) {
    char *it;
    for (it = pch; *it != ':' && *it != 0; it ++);
    if (*it == 0) return false;
    *it = 0;

    if (!strtou64(pch, &idx)) return false;
    if (!strtofloat(it+1, &val)) return false;
    if (last_idx > idx) return false;
    last_idx = idx;

    grp->add_fea_id(idx);
    grp->add_fea_val(val);
    pch = strtok_r(NULL, " \t\r\n", &saveptr);
  }
  return true;
}

// adfea format:
//
//   line_id 1 clicked_or_not fea_id:grp_id fea_id:grp_id ...
//
// same group_ids should appear together, but not necesary be ordered
bool TextParser::parseAdfea(char* line, Instance* ins) {
  uint64 fea_id = -1;
  int pre_grp_id = -1;
  FeatureGroup* grp = nullptr;

  char *saveptr;
  char* tk = strtok_r(line, " :", &saveptr);
  for (int i = 0; tk != NULL; tk = strtok_r(NULL, " :", &saveptr), ++i) {
    if (i == 0) {
      // skip it the ins id
    } else if (i == 1) {
      // skip, it is 1
    } else if (i == 2) {
      int32 label;
      if (!strtoi32(tk, &label)) return false;
      ins->set_label(label > 0 ? 1 : -1);
    } else if (i % 2 == 1) {
      if (!strtou64(tk, &fea_id)) return false;
    } else {
      int grp_id = 0;
      if (!ignore_fea_grp_ && !strtoi32(tk, &grp_id)) return false;
      if (grp_id != pre_grp_id) {
        grp = ins->add_fea_grp();
        grp->set_grp_id(grp_id);
        pre_grp_id = grp_id;
      }
      grp->add_fea_id(fea_id);
    }
  }
  // LL << ins->ShortDebugString();
  return true;
}

// terafea format:
//
//   clicked_or_not line_id | uint64 uint64 ...
//   uint64:
//      the most significant 10 bits    - group id
//      lower 54 bits                   - feature id
//
//  no guarantee that the same group ids stay contiguously
//
bool TextParser::parseTerafea(char* line, Instance* ins) {
  // key:   group id
  // value: index in Example::slot[]
  std::unordered_map<uint32, uint32> gid_idx_map;

  char *saveptr;
  char* tk = strtok_r(line, " :", &saveptr);
  for (int i = 0; tk != NULL; tk = strtok_r(NULL, " :", &saveptr), ++i) {
    if (i == 0) {
      // label
      int32 label;
      if (!strtoi32(tk, &label)) return false;
      ins->set_label(label > 0 ? 1 : -1);
    } else if (i == 1) {
      // skip, line_id
    } else if (i == 2) {
      // skip, seperator
    } else {
      uint64 key = -1;
      if (!strtou64(tk, &key)) return false;

      uint64 grp_id = key >> 54;
      uint64 fea_id = key & 0x3FFFFFFFFFFFFF;

      if (FLAGS_shuffle_fea_id) {
        uint64 murmur_out[2];
        MurmurHash3_x64_128(&fea_id, 8, 512927377, murmur_out);
        fea_id = (murmur_out[0] ^ murmur_out[1]);
      }

      FeatureGroup *grp = nullptr;
      auto iter = gid_idx_map.find(grp_id);
      if (gid_idx_map.end() != iter) {
        grp = ins->mutable_fea_grp(iter->second);
      } else {
        // register in the map
        gid_idx_map[grp_id] = ins->fea_grp_size();
        // add new FeatureGroup in Instance
        grp = ins->add_fea_grp();
        grp->set_grp_id(grp_id);
      }
      grp->add_fea_id(fea_id);
    }
  }
  // LL << ins->ShortDebugString();
  return true;
}

// ps format: TODO
//
// label; group_id feature[:weight] feature[:weight] ...; groud_id ...; ...
//
// - label: the label of the instance. integer for classification, float for
//   regression, and emtpy for unsupervised learning
//
// - group_id: the integer identity of a feature group, each instance should
//   contains at least one feature group.
//
// - feature: an 64-bit integer presenting the feature id for sparse training
//   data, an float feature value for dense training data.
//
// - weight: only valid for non-bianry sparse training data, a float number
//   presenting the feature value.
//
//
// vw: TODO
//
} // namespace PS
