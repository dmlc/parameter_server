#include "data/parse_text.h"
#include <functional>
#include "util/strtonum.h"
#include "util/resource_usage.h"
#include "base/matrix_io_inl.h"

namespace PS {

ParseText::ParseText(TextFormat format, bool ignore_feature_group = false) {
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
  ++ num_ins_;
  nnz_ele_ += ins->fea_id_size();

  // update info
  if (ignore_fea_grp_) return true;
  if (ins->grp_id_size() != ins->fea_id_size()) return false;
  int32 pre_grp_id = -1;
  for (int i = 0; i < ins->fea_id_size(); ++i) {
    int32 grp_id = ins->grp_id(i);
    auto& ginfo = grp_info_[grp_id];
    if (grp_id != pre_grp_id) {
      ginfo.set_nnz_ins(ginfo.nnz_ins() + 1);
      pre_grp_id = grp_id;
    }
    // ginfo.set_fea_begin(std::min((uint64)ginfo.fea_begin(), fea_id));
    // ginfo.set_fea_end(std::max((uint64)ginfo.fea_end(), fea_id + 1));
    ginfo.set_nnz_ele(ginfo.nnz_ele() + 1);
  }
  return true;
}

InstanceInfo ParseText::info() {
  info_.clear_fea_group();
  if (ignore_fea_grp_) {
    grp_info_[0].set_id(0);
    grp_info_[0].set_nnz_ele(nnz_ele_);
    grp_info_[0].set_nnz_ins(num_ins_);
  }
  for (auto& it : grp_info_) {
    it.second.set_id(it.first);
    *info_.add_fea_grp() = it.second;
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

  while (pch != NULL) {
    char *it;
    for (it = pch; *it != ':' && *it != 0; it ++);
    if (*it == 0) return false;
    *it = 0;

    if (!strtou64(pch, &idx)) return false;
    if (!strtofloat(it+1, &val)) return false;
    if (last_idx > idx) return false;
    last_idx = idx;

    if (!encode(idx, 0, &idx)) return false;
    ins->add_fea_id(idx);
    ins->add_fea_val(val);
    pch = strtok (NULL, " \t\r\n");
  }
  return true;
}

// adfea format:
//
//   line_id 1 clicked_or_not fea_id:group_id fea_id:group_id ...
//
// same group_ids are appear together, but not necesary be ordered
bool ParseText::parseAdfea(char* line, Instance* ins) {
  std::vector<uint64> feas;
  uint64 fea_id = 0;
  char* tk = strtok (line, " :");
  for (int i = 0; tk != NULL; tk = strtok (NULL, " :"), ++i) {
    uint64 num;
    if (!strtou64(tk, &num)) return false;
    if (i == 0) {
      ins->set_ins_id(num);
    } else if (i == 1) {
      // skip,
    } else if (i == 2) {
      ins->set_label(num > 0 ? 1 : -1);
    } else if (i % 2 == 1) {
      ins->add_fea_id(num);
    } else {
      if (!ignore_fea_grp_) ins->add_grp_id((int32)num);
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
