#include "data/parse_text.h"
#include <functional>
#include "util/strtonum.h"
#include "util/resource_usage.h"
#include "base/matrix_io_inl.h"

namespace PS {

ParseText::ParseText(TextFormat format, bool ignore_feature_group) {
  ignore_fea_grp_ = ignore_feature_group;
  using namespace std::placeholders;
  switch (format) {
    case DataConfig::LIBSVM:
      ignore_fea_grp_ = true;
      convertor_ = std::bind(&ParseText::parseLibsvm, this, _1, _2);
      info_.set_fea_type(InstanceInfo::SPARSE);
      info_.set_label_type(InstanceInfo::BINARY);
      break;
    case DataConfig::ADFEA:
      convertor_ = std::bind(&ParseText::parseAdfea, this, _1, _2);
      info_.set_fea_type(InstanceInfo::SPARSE_BINARY);
      info_.set_label_type(InstanceInfo::BINARY);
      break;
    default:
      CHECK(false) << "unknow text format " << format;
  }
}

bool ParseText::toProto(char* line, Instance* ins) {
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
  ++ num_ins_;
  return true;
}

InstanceInfo ParseText::info() {
  info_.clear_fea_grp();
  info_.set_num_ins(num_ins_);
  for (int i = 0; i < kGrpIDmax; ++i) {
    if (grp_info_[i].has_grp_id()) {
      *info_.add_fea_grp() = grp_info_[i];
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

bool ParseText::parseLibsvm(char* buff, Instance* ins) {
  char * pch = strtok (buff, " \t\r\n");
  uint64 idx, last_idx=0;
  float label, val;

  if (!strtofloat(pch, &label)) return false;
  ins->set_label(label);
  if (floor(label) != label) {
    info_.set_label_type(InstanceInfo::CONTINUOUS);
  } else if (label != 1 && label != -1) {
    info_.set_label_type(InstanceInfo::MULTICLASS);
  }
  pch = strtok (NULL, " \t\r\n");

  auto grp = ins->add_fea_grp();
  grp->set_grp_id(0);
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
    pch = strtok (NULL, " \t\r\n");
  }
  return true;
}

// adfea format:
//
//   line_id 1 clicked_or_not fea_id:grp_id fea_id:grp_id ...
//
// same group_ids should appear together, but not necesary be ordered
bool ParseText::parseAdfea(char* line, Instance* ins) {
  std::vector<uint64> feas;
  uint64 fea_id;
  int pre_grp_id = -1;
  FeatureGroup* grp = nullptr;

  char* tk = strtok (line, " :");
  for (int i = 0; tk != NULL; tk = strtok (NULL, " :"), ++i) {
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
      }
      grp->add_fea_id(fea_id);
    }
  }
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
