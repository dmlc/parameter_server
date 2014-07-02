#include "data/text2bin.h"
#include "util/resource_usage.h"
#include "base/matrix_io.h"
#include <bitset>

// TODO if input is emtpy, use std::in
DEFINE_string(input, "../data/ps.txt", "input file name");
DEFINE_string(output, "../data/ps", "output file name");

DEFINE_uint64(format_detector, 100,
              "using the first #format_detector lines to detect the format");
DEFINE_bool(compression, false, "compress each instance" );
DEFINE_bool(verbose, true, "");
DEFINE_string(text_format, "pserver", "pserver, libsvm, vw or adfea");
DEFINE_string(bin_format, "protobuf", "protobuf, or binary");

namespace PS {
// TODO support binary formart
void Text2Bin::init() {
  string record_name = FLAGS_output + ".recordio";
  record_file_ = File::openOrDie(record_name, "w");
  record_writer_ = unique_ptr<RecordWriter>(new RecordWriter(record_file_));
  record_writer_->set_use_compression(FLAGS_compression);

  text_format_ = FLAGS_text_format;
  std::transform(text_format_.begin(), text_format_.end(),
                 text_format_.begin(), ::tolower);

  if (adfea()) {
    info_.mutable_all_group()->set_feature_type(FeatureGroupInfo::SPARSE_BINARY);
    info_.set_label_type(InstanceInfo::BINARY);
  } else if (libsvm()) {
    info_.mutable_all_group()->set_feature_type(FeatureGroupInfo::SPARSE);
    info_.set_label_type(InstanceInfo::BINARY);
  }
}

void Text2Bin::finalize() {
  if (!file_header_.empty()) {
    if (pserver()) {
      for (auto& l : file_header_) num_line_written_ += parsePServerInstance(l);
    }
    file_header_.clear();
  }

  writeInfo();
}

void Text2Bin::writeInfo() {
  info_.mutable_all_group()->set_group_id(-1);
  info_.clear_individual_groups();
  for (auto& it : group_info_) {
    auto& g = it.second;
    g.set_feature_type(info_.all_group().feature_type());
    g.set_group_id(it.first);
    g.set_num_instances(num_line_written_);
    *info_.add_individual_groups() = g;
    *info_.mutable_all_group() = mergeFeatureGroupInfo(info_.all_group(), g);
  }

  WriteProtoToASCIIFileOrDie(info_, FLAGS_output + ".info");

  InstanceInfo f;
  CHECK(ReadFileToProto(FLAGS_output + ".info", &f))
      << " you may set a larger value to fix this problem, say -format_detector 100000";

}

bool Text2Bin::parseLabel(const string& label_str, float* label) {
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

bool Text2Bin::detectLabel(const string& label_str, float* label) {
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

void Text2Bin::processAdFea(char *line) {
  ++ num_line_processed_;
  Instance instance;
  std::vector<uint64> feas;
  char* tk = strtok (line, " :");
  uint64 fea_id = 0;
  for (int i = 0; tk != NULL; tk = strtok (NULL, " :"), ++i) {
    if (i <= 1) continue;
    uint64 num;
    if (!strtou64(tk, &num)) return;
    if (i == 2) {
      instance.set_label(num);
    } else if (i % 2 == 1) {
      fea_id = num;
    } else {
      uint64 group_id = (num << 62) | ((num & 0x3C) << 56) | ((num & 0x3C0) << 48);
      // uint64 group_id = num << 54;
      fea_id = (fea_id >> 10) | group_id;

      feas.push_back(fea_id);

      auto& ginfo = group_info_[num];
      ginfo.set_feature_begin(std::min((uint64)ginfo.feature_begin(), fea_id));
      ginfo.set_feature_end(std::max((uint64)ginfo.feature_end(), fea_id + 1));
      ginfo.set_num_entries(ginfo.num_entries() + 1);
    }
  }

  std::sort(feas.begin(), feas.end());
  for (auto& f : feas) instance.add_feature_id(f);

  CHECK(record_writer_->WriteProtocolMessage(instance));

  ++ num_line_written_;
}

bool Text2Bin::parsePServerInstance(const string& line) {

  Instance instance;
  // do not skip empty group
  auto group = split(line, ';', false);
  if (group.size() < 2) {
    LL << "an instacne should have a (may empty) label and at least one feature group";
    goto PARSE_PS_ERROR;
  }

  // parse label
  if (!group[0].empty()) {
    float label;
    if (!parseLabel(group[0], &label)) goto PARSE_PS_ERROR;
    instance.set_label(label);
  }

  // parse feature group
  for (int i = 1; i < group.size(); ++i) {
    auto item = split(group[i], ' ', true);
    if (item.size() < 2) {
      LL << "a feature group should have at least one feature";
      goto PARSE_PS_ERROR;
    }

    int group_id;
    if (!strtoi32(item[0], &group_id)) {
      LL << "fail to parse group id: " << item[0];
      goto PARSE_PS_ERROR;
    }

    typedef FeatureGroupInfo FInfo;
    uint64 feature_id;
    float value;
    auto& ginfo = group_info_[group_id];

    const auto& type = info_.all_group().feature_type();
    for (size_t j = 1; j < item.size(); ++j) {
      if (type == FInfo::SPARSE_BINARY) {
        // format: feature_id feature_id ...
        if (!strtou64(item[j], &feature_id)) {
          LL << "fail to parse feature id: " << item[j];
          goto PARSE_PS_ERROR;
        }
        instance.add_feature_id(feature_id);
      } else if (type == FInfo::SPARSE) {
        auto fea = split(item[j], ':', true);
        if (fea.size() != 2) {
          LL << "the format of sparse feature is feature_id:value, invalid "
             << item[j];
          goto PARSE_PS_ERROR;
        }
        if (!strtou64(fea[0], &feature_id)) {
          LL << "fail to parse feature id: " << fea[0];
          goto PARSE_PS_ERROR;
        }
        instance.add_feature_id(feature_id);
        if (!strtofloat(fea[1], &value)) {
          LL << "fail to parse value: " << fea[1];
          goto PARSE_PS_ERROR;
        }
        instance.add_value(value);
      } else if (type == FInfo::DENSE) {
        float value;
        if (!strtofloat(item[j], &value)) {
          LL << "fail to parse value: " << item[j];
          goto PARSE_PS_ERROR;
        }
        instance.add_value(value);
      }
      ginfo.set_feature_begin(std::min((uint64)ginfo.feature_begin(), feature_id));
      ginfo.set_feature_end(std::max((uint64)ginfo.feature_end(), feature_id + 1));
    }
    ginfo.set_num_entries(ginfo.num_entries() + item.size() - 1);
  }
  return record_writer_->WriteProtocolMessage(instance);

PARSE_PS_ERROR:
  LL << "failed to parse line " << num_line_processed_
     << " of " << FLAGS_input << ": " << line;
  return false;
}

bool Text2Bin::detectPServerInstance(const string& line) {
  auto group = split(line, ';', false);
  if (group.size() < 2) return false;

  // parse label
  float label; if (!detectLabel(group[0], &label)) return false;

  // parse feature group
  for (int i = 1; i < group.size(); ++i) {
    auto item = split(group[i], ' ', true);
    if (item.size() < 2) return false;

    for (int j = 1; j < item.size(); ++j) {
      typedef FeatureGroupInfo FInfo;
      uint64 feature_id;
      auto fea = split(item[j], ':', true);
      auto g = info_.mutable_all_group();
      if (fea.size() == 2) {
        if (g->has_feature_type()) CHECK_EQ(g->feature_type(), FInfo::SPARSE);
        g->set_feature_type(FInfo::SPARSE);
      } else if (fea.size() == 1) {
        if (strtou64(item[j], &feature_id)) {
          if (!g->has_feature_type()) g->set_feature_type(FInfo::SPARSE_BINARY);
        } else {
          g->set_feature_type(FInfo::DENSE);
        }
      } else {
        return false;
      }
    }
  }
  return true;
}

void Text2Bin::processPServer(char *line) {
  string line_str(line);
  if (num_line_processed_ < FLAGS_format_detector) {
    if (!detectPServerInstance(line_str)) LL << "detect format error: " << line_str;
    file_header_.push_back(line_str);
  } else {
    if (!file_header_.empty()) {
      for (auto& l : file_header_) num_line_written_ += parsePServerInstance(l);
      file_header_.clear();
    }
    num_line_written_ += parsePServerInstance(line_str);
  }
  ++ num_line_processed_;
}

void Text2Bin::processLibSvm(char *buff) {
  ++ num_line_processed_;
  Instance instance;

  char * pch = strtok (buff, " \t\r\n");
  int cnt = 0;
  uint64 idx, last_idx=0;
  float label, val;

  std::vector<uint64> feas;
  std::vector<float> vals;

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
      ginfo.set_feature_begin(std::min((uint64)ginfo.feature_begin(), idx));
      ginfo.set_feature_end(std::max((uint64)ginfo.feature_end(), idx + 1));
      ginfo.set_num_entries(ginfo.num_entries() + 1);

      instance.add_feature_id(idx);
      instance.add_value(val);
    }
    pch = strtok (NULL, " \t\r\n");
    cnt ++; 
  }

  CHECK(record_writer_->WriteProtocolMessage(instance));
  ++ num_line_written_;
}

} // namespace PS

int main(int argc, char *argv[]) {
  using namespace PS;
  FLAGS_logtostderr = 1;
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  Text2Bin convertor;

  convertor.init();

  if (FLAGS_verbose) {
    std::cout << "read " << FLAGS_input << " in format " << FLAGS_text_format << std::endl;
  }

  FileLineReader reader(FLAGS_input.c_str());

  if (convertor.pserver())
    reader.set_line_callback([&convertor] (char *line) { convertor.processPServer(line); });
  else if (convertor.libsvm())
    reader.set_line_callback([&convertor] (char *line) { convertor.processLibSvm(line); });
  else if (convertor.vw())
    reader.set_line_callback([&convertor] (char *line) { convertor.processVW(line); });
  else if (convertor.adfea())
    reader.set_line_callback([&convertor] (char *line) { convertor.processAdFea(line); });
  else
    CHECK(false) << "unknow text format " << FLAGS_text_format;

  Timer t; t.start();
  reader.Reload();

  convertor.finalize();

  if (FLAGS_verbose) {
    std::cout << "written " << convertor.num_lines_written() << " instances into " << FLAGS_output
      << ".recordio, with speed "
      << convertor.num_lines_written() / t.get()  << " per second, "
      << " skipped " << convertor.num_lines_skipped() << std::endl;
  }
  return 0;
}
