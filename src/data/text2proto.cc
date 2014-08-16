#include "data/text2proto.h"
#include "util/resource_usage.h"
#include "base/matrix_io.h"
// #include <bitset>

// TODO if input is emtpy, use std::in
// DEFINE_string(input, "../data/ps.txt", "input file name");
// DEFINE_string(output, "../data/ps", "output file name");

DEFINE_uint64(format_detector, 1000,
              "using the first *format_detector* lines to detect the format");
DEFINE_bool(verbose, true, "");
DEFINE_string(format, "none", "pserver, libsvm, vw or adfea");

namespace PS {

void Text2Proto::init() {
  auto file = File::openOrDie("stdout", "w");
  writer_ = unique_ptr<RecordWriter>(new RecordWriter(file));

  format_ = FLAGS_format;
  std::transform(format_.begin(), format_.end(), format_.begin(), ::tolower);

  if (adfea()) {
    info_.set_fea_type(FeatureGroupInfo::SPARSE_BINARY);
    info_.set_label_type(InstanceInfo::BINARY);
  } else if (libsvm()) {
    info_.set_fea_type(FeatureGroupInfo::SPARSE);
    info_.set_label_type(InstanceInfo::BINARY);
  }
}

void Text2Proto::write() {
  CHECK(!group_info_.emtpy());
  info_.set_num_ins(num_good_record_);
  for (auto& it : group_info_) {
    *info_.add_fea_group() = it.second;
  }
  writer_.WriteProtocolMessage(info_);
}

// input format:
// line_id 1 click fea_id:group_id fea_id:group_id ...
void Text2Proto::processAdFea(char *line) {
  ++ num_processed_;

  Instance ins;
  std::vector<uint64> feas;
  uint64 fea_id = 0;

  char* tk = strtok (line, " :");
  for (int i = 0; tk != NULL; tk = strtok (NULL, " :"), ++i) {
    uint64 num;
    if (!strtou64(tk, &num)) return;
    if (i == 0) {
      ins.set_ins_id(num);
    } else if (i == 1) {
      // skip
    } else if (i == 2) {
      ins.set_label(num > 0 ? 1 : -1);
    } else if (i % 2 == 1) {
      fea_id = num;
    } else {
      // assume the group_id has format   x...xaaaabbbbcc
      // and the fea_id                   d...dx........x
      // then the new feature is          ccbbbbaaaad...d,
      // so that the feature ranges for different groups are not overlapped
      if (num >= 1024) return;
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
  for (auto& f : feas) ins.add_fea_id(f);
  records_.push_back(ins);
}

void Text2Proto::processLibSvm(char *buff) {
  ++ num_processed_;
  Instance instance;

  char * pch = strtok (buff, " \t\r\n");
  int cnt = 0;
  uint64 idx, last_idx=0;
  float label, val;

  while (pch != NULL) {
    if (cnt == 0) {
      if (!strtofloat(pch, &label)) return;
      instance.set_label(label);
      if (floor(label) != label) {
        info_.set_label_type(InstanceInfo::CONTINUOUS);
      } else if (label != 1 && label != -1) {
        info_.set_label_type(InstanceInfo::MULTICLASS);
      }
    } else {
      char *it;
      for (it = pch; *it != ':' && *it != 0; it ++);
      if (*it == 0) return;
      *it = 0;

      if (!strtou64(pch, &idx)) return;
      if (!strtofloat(it+1, &val)) return;
      if (last_idx > idx) return;
      last_idx = idx;

      auto& ginfo = group_info_[1];
      ginfo.set_fea_begin(std::min((uint64)ginfo.fea_begin(), idx));
      ginfo.set_fea_end(std::max((uint64)ginfo.fea_end(), idx + 1));
      ginfo.set_nnz(ginfo.nnz() + 1);

      instance.add_fea_id(idx);
      instance.add_fea_val(val);
    }
    pch = strtok (NULL, " \t\r\n");
    cnt ++;
  }
  records_.push_back(instance);
}


bool Text2Proto::parseLabel(const string& label_str, float* label) {
  typedef InstanceInfo IInfo;
  if (label_str.empty()) {
    if (info_.label_type() != IInfo::EMPTY) {
      LL << "label should be empty";
      return false;
    }
  } else {
    if (!strtofloat(label_str, label)) {
      LL << "fail to parse label as float: " << label_str;
      return false;
    }
    if (info_.label_type() == IInfo::BINARY) {
      if (*label != -1 && *label != 1) {
        LL << "expect a bianry label: " << *label;
        return false;
      }
    } else if (info_.label_type() == IInfo::MULTICLASS) {
      if (floor(*label) != *label) {
        LL << "expect an integer label: " << label;
        return false;
      }
    }
  }
  return true;
}

bool Text2Proto::detectLabel(const string& label_str, float* label) {
  typedef InstanceInfo IInfo;
  if (label_str.empty()) {
    info_.set_label_type(IInfo::EMPTY);
  } else {
    if (!strtofloat(label_str, label)) return false;

    auto type = info_.label_type();
    if (*label == -1 || *label == 1) {
      if (type != IInfo::MULTICLASS && type != IInfo::CONTINUOUS)
        info_.set_label_type(IInfo::BINARY);
    } else if (floor(*label) == *label) {
      if (type != IInfo::CONTINUOUS)
        info_.set_label_type(IInfo::MULTICLASS);
    } else {
      info_.set_label_type(IInfo::CONTINUOUS);
    }
  }
  return true;
}

bool Text2Proto::parsePServerInstance(const string& line) {
//   Instance instance;
//   // do not skip empty group
//   auto group = split(line, ';', false);
//   if (group.size() < 2) {
//     LL << "an instacne should have a (may empty) label and at least one feature group";
//     goto PARSE_PS_ERROR;
//   }

//   // parse label
//   if (!group[0].empty()) {
//     float label;
//     if (!parseLabel(group[0], &label)) goto PARSE_PS_ERROR;
//     instance.set_label(label);
//   }

//   // parse feature group
//   for (int i = 1; i < group.size(); ++i) {
//     auto item = split(group[i], ' ', true);
//     if (item.size() < 2) {
//       LL << "a feature group should have at least one feature";
//       goto PARSE_PS_ERROR;
//     }

//     int group_id;
//     if (!strtoi32(item[0], &group_id)) {
//       LL << "fail to parse group id: " << item[0];
//       goto PARSE_PS_ERROR;
//     }

//     typedef FeatureGroupInfo FInfo;
//     uint64 feature_id;
//     float value;
//     auto& ginfo = group_info_[group_id];

//     const auto& type = info_.all_group().feature_type();
//     for (size_t j = 1; j < item.size(); ++j) {
//       if (type == FInfo::SPARSE_BINARY) {
//         // format: feature_id feature_id ...
//         if (!strtou64(item[j], &feature_id)) {
//           LL << "fail to parse feature id: " << item[j];
//           goto PARSE_PS_ERROR;
//         }
//         instance.add_feature_id(feature_id);
//       } else if (type == FInfo::SPARSE) {
//         auto fea = split(item[j], ':', true);
//         if (fea.size() != 2) {
//           LL << "the format of sparse feature is feature_id:value, invalid "
//              << item[j];
//           goto PARSE_PS_ERROR;
//         }
//         if (!strtou64(fea[0], &feature_id)) {
//           LL << "fail to parse feature id: " << fea[0];
//           goto PARSE_PS_ERROR;
//         }
//         instance.add_feature_id(feature_id);
//         if (!strtofloat(fea[1], &value)) {
//           LL << "fail to parse value: " << fea[1];
//           goto PARSE_PS_ERROR;
//         }
//         instance.add_value(value);
//       } else if (type == FInfo::DENSE) {
//         float value;
//         if (!strtofloat(item[j], &value)) {
//           LL << "fail to parse value: " << item[j];
//           goto PARSE_PS_ERROR;
//         }
//         instance.add_value(value);
//       }
//       ginfo.set_feature_begin(std::min((uint64)ginfo.feature_begin(), feature_id));
//       ginfo.set_feature_end(std::max((uint64)ginfo.feature_end(), feature_id + 1));
//     }
//     ginfo.set_num_entries(ginfo.num_entries() + item.size() - 1);
//   }
//   return record_writer_->WriteProtocolMessage(instance);
// PARSE_PS_ERROR:
//   LL << "failed to parse line " << num_line_processed_
//      << " of " << FLAGS_input << ": " << line;
//   return false;
}

bool Text2Proto::detectPServerInstance(const string& line) {
  // auto group = split(line, ';', false);
  // if (group.size() < 2) return false;

  // // parse label
  // float label; if (!detectLabel(group[0], &label)) return false;

  // // parse feature group
  // for (int i = 1; i < group.size(); ++i) {
  //   auto item = split(group[i], ' ', true);
  //   if (item.size() < 2) return false;

  //   for (int j = 1; j < item.size(); ++j) {
  //     typedef FeatureGroupInfo FInfo;
  //     uint64 feature_id;
  //     auto fea = split(item[j], ':', true);
  //     auto g = info_.mutable_all_group();
  //     if (fea.size() == 2) {
  //       if (g->has_feature_type()) CHECK_EQ(g->feature_type(), FInfo::SPARSE);
  //       g->set_feature_type(FInfo::SPARSE);
  //     } else if (fea.size() == 1) {
  //       if (strtou64(item[j], &feature_id)) {
  //         if (!g->has_feature_type()) g->set_feature_type(FInfo::SPARSE_BINARY);
  //       } else {
  //         g->set_feature_type(FInfo::DENSE);
  //       }
  //     } else {
  //       return false;
  //     }
  //   }
  // }
  // return true;
}

void Text2Proto::processPServer(char *line) {
  // string line_str(line);
  // if (num_line_processed_ < FLAGS_format_detector) {
  //   if (!detectPServerInstance(line_str)) LL << "detect format error: " << line_str;
  //   file_header_.push_back(line_str);
  // } else {
  //   if (!file_header_.empty()) {
  //     for (auto& l : file_header_) num_line_written_ += parsePServerInstance(l);
  //     file_header_.clear();
  //   }
  //   num_line_written_ += parsePServerInstance(line_str);
  // }
  // ++ num_line_processed_;
}

} // namespace PS

int main(int argc, char *argv[]) {
  using namespace PS;
  FLAGS_logtostderr = 1;
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  if (FLAGS_verbose) {
    std::cerr << "parse input as " << FLAGS_text_format << " format\n";
  }
  Text2Proto convertor;
  convertor.init();

  Timer t; t.start();
  FileLineReader reader("stdin");
  if (convertor.pserver()) {
    reader.set_line_callback([&convertor] (char *line) {
        convertor.processPServer(line);
      });
  } else if (convertor.libsvm()) {
    reader.set_line_callback([&convertor] (char *line) {
        convertor.processLibSvm(line);
      });
  } else if (convertor.vw()) {
    reader.set_line_callback([&convertor] (char *line) {
        convertor.processVW(line);
      });
  } else if (convertor.adfea()) {
    reader.set_line_callback([&convertor] (char *line) {
        convertor.processAdFea(line);
      });
  } else {
    CHECK(false) << "unknow text format " << FLAGS_text_format;
  }
  reader.Reload();

  convertor.finalize();

  if (FLAGS_verbose) {
    std::cerr << "written " << convertor.num_lines_written()
              << " instances in " <<  t.get()  << " sec." << std::endl;
    if (convertor.num_lines_skipped()) {
      std::cerr << convertor.num_lines_skipped()
                << " bad instances are skipped" << std::endl;
    }
  }
  return 0;
}
