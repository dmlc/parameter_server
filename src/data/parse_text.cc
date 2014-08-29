#include "data/parse_text.h"
#include <functional>
#include "util/strtonum.h"
#include "util/resource_usage.h"
#include "base/matrix_io_inl.h"

namespace PS {

bool ParseText::toProto(char* line, Instance* ins) {
  ins->Clear();
  bool ret = convertor_(line, ins);
  info_.set_num_ins(info_.num_ins() + 1);
  return ret;
}

InstanceInfo ParseText::info() {
  info_.clear_fea_group();
  FeatureGroupInfo g;
  for (auto& it : group_info_) {
    g = mergeFeatureGroupInfo(g, it.second);
  }
  g.set_group_id(-1);
  *info_.add_fea_group() = g;
  for (auto& it : group_info_) {
    it.second.set_group_id(it.first);
    *info_.add_fea_group() = it.second;
  }
  return info_;
}

void ParseText::setFormat(TextFormat format) {
  using namespace std::placeholders;
  switch (format) {
    case DataConfig::LIBSVM:
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

// libsvm:
// label feature_id:weight feature_id:weight feature_id:weight ...
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

    auto& ginfo = group_info_[1];
    ginfo.set_fea_begin(std::min((uint64)ginfo.fea_begin(), idx));
    ginfo.set_fea_end(std::max((uint64)ginfo.fea_end(), idx + 1));
    ginfo.set_nnz(ginfo.nnz() + 1);

    ins->add_fea_id(idx);
    ins->add_fea_val(val);
    pch = strtok (NULL, " \t\r\n");
  }
  return true;
}


// adfea:
// line_id 1 click fea_id:group_id fea_id:group_id ...
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
      // skip
    } else if (i == 2) {
      ins->set_label(num > 0 ? 1 : -1);
    } else if (i % 2 == 1) {
      fea_id = num;
    } else {
      // assume the group_id has format   x...xaaaabbbbcc
      // and the fea_id                   d...dx........x
      // then the new feature is          ccbbbbaaaad...d,
      // so that the feature ranges for different groups are not overlapped
      if (num >= 1024) return false;
      uint64 group_id = (num << 62) | ((num & 0x3C) << 56) | ((num & 0x3C0) << 48);
      fea_id = (fea_id >> 10) | group_id;

      feas.push_back(fea_id);
      auto& ginfo = group_info_[num];
      ginfo.set_fea_begin(std::min((uint64)ginfo.fea_begin(), fea_id));
      ginfo.set_fea_end(std::max((uint64)ginfo.fea_end(), fea_id + 1));
      ginfo.set_nnz(ginfo.nnz() + 1);
    }
  }
  std::sort(feas.begin(), feas.end());
  for (auto& f : feas) ins->add_fea_id(f);
  return true;
}



// ps format:
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
