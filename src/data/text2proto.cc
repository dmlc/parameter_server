#include "data/text2proto.h"

// TODO if input is emtpy, use std::in
DEFINE_string(input, "../data/ps.txt", "input file name");
DEFINE_string(output, "../data/ps", "output file name");

DEFINE_uint64(format_detector, 100000,
              "using the first #format_detector lines to detect the format");
DEFINE_bool(compression, false, "compress each instance" );
DEFINE_bool(verbose, true, "");
DEFINE_string(text_format, "pserver", "pserver, libsvm, vw");

namespace PS {

void Text2Pb::init() {
  string record_name = FLAGS_output + ".recordio";
  record_file_ = File::OpenOrDie(record_name, "w");
  record_writer_ = unique_ptr<RecordWriter>(new RecordWriter(record_file_));
  record_writer_->set_use_compression(FLAGS_compression);

  text_format_ = FLAGS_text_format;
  std::transform(text_format_.begin(), text_format_.end(),
                 text_format_.begin(), ::tolower);

  if (libsvm()) {
    group_info_[0].set_group_id(0);
    group_info_[0].set_feature_type(FeatureGroupInfo::SPARSE);
    group_info_[0].set_num_entries(0);
  }
}

void Text2Pb::finalize() {
  if (!file_header_.empty()) {
    if (pserver()) {
      for (auto& l : file_header_) num_line_written_ += parsePServerInstance(l);
    } else {
      // TODO
      CHECK(false);
    }
    file_header_.clear();
  }

  writeInfo();

  if (FLAGS_verbose) {
    std::cout << "written " << num_line_written_ << " examples into " << FLAGS_output
              << ".recordio, skipped " << num_line_processed_-num_line_written_
              << std::endl;
  }
}

void Text2Pb::writeInfo() {
  info_.set_num_examples(num_line_written_);

  uint64 fea_min = kuint64max, fea_max = 0;
  info_.clear_feature_group_info();
  for (auto& it : group_info_) {
    it.second.set_num_examples(num_line_written_);
    *info_.add_feature_group_info() = it.second;
    fea_min = std::min(fea_min, (uint64)it.second.feature_begin());
    fea_max = std::max(fea_max, (uint64)it.second.feature_end());
  }
  info_.set_feature_begin(fea_min);
  info_.set_feature_end(fea_max);

  WriteProtoToASCIIFileOrDie(info_, FLAGS_output + ".info");

  PServerInputInfo f;
  // ReadFileToProtoOrDie(FLAGS_output + ".info", &f);
  CHECK(ReadFileToProto(FLAGS_output + ".info", &f))
      << " you may set a larger value to fix this problem, say -format_detector 100000";

}

bool Text2Pb::parseLabel(const string& label_str, float* label) {
  typedef PServerInputInfo IInfo;
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

bool Text2Pb::detectLabel(const string& label_str, float* label) {
  typedef PServerInputInfo IInfo;
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

void Text2Pb::processLibSvm(char *line) {

  num_line_processed_++;

  PServerInput instance;
  auto fg = instance.add_feature_group();
  fg->set_group_id(0);
  uint64 fea_min = kuint64max, fea_max = 0;

  auto item = split(string(line), ' ', true);
  if (item.size() < 2) {
    LL << "an instance should contains at least one feature";
    goto PARSE_LS_ERROR;
  }

  float label;
  if (num_line_processed_ < FLAGS_format_detector) {
    if (!detectLabel(item[0], &label)) goto PARSE_LS_ERROR;
  } else {
    if (!parseLabel(item[0], &label)) goto PARSE_LS_ERROR;
  }
  instance.set_label(label);

  for (int i = 1; i < item.size(); ++i) {
    auto fea = split(item[i], ':', true);
    if (fea.size() != 2) {
      LL << "the format of a feature is feature_id:value";
      goto PARSE_LS_ERROR;
    }
    uint64 feature_id;
    float value;
    if (!strtou64(fea[0], &feature_id)) {
      LL << "fail to parse feature id: " << fea[0];
      goto PARSE_LS_ERROR;
    }
    fg->add_feature_id(feature_id);

    if (!strtofloat(fea[1], &value)) {
      LL << "fail to parse value: " << fea[1];
      goto PARSE_LS_ERROR;
    }
    fg->add_value(value);

    fea_min = std::min(fea_min, feature_id);
    fea_max = std::max(fea_max, feature_id);
  }

  group_info_[0].set_feature_begin(fea_min);
  group_info_[0].set_feature_end(fea_max);
  group_info_[0].set_num_entries(group_info_[0].num_entries() + item.size() - 1);

  // write into file
  record_writer_->WriteProtocolMessage(instance);

  num_line_written_ ++;
  return;

PARSE_LS_ERROR:
  LL << "failed to parse line " << num_line_processed_
     << " of " << FLAGS_input << ": " << line;
  return;
}

bool Text2Pb::parsePServerInstance(const string& line) {

  PServerInput instance;
  // do not skip empty group
  auto group = split(line, ';', false);
  if (group.size() < 2) {
    LL << "an instacne should have a (empty) label and at least one group";
    goto PARSE_PS_ERROR;
  }

  // parse label
  float label;
  if (!parseLabel(group[0], &label)) goto PARSE_PS_ERROR;
  if (!group[0].empty()) instance.set_label(label);

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
    auto type = group_info_[group_id].feature_type();
    auto fg = instance.add_feature_group();
    fg->set_group_id(group_id);

    uint64 fea_min = kuint64max, fea_max = 0;
    typedef FeatureGroupInfo FInfo;
    if (type == FInfo::SPARSE_BINARY) {
      // format: feature_id feature_id ...
      for (size_t j = 1; j < item.size(); ++j) {
        uint64 feature_id;
        if (!strtou64(item[j], &feature_id)) {
          LL << "fail to parse feature id: " << item[j];
          goto PARSE_PS_ERROR;
        }
        fg->add_feature_id(feature_id);
        fea_min = std::min(fea_min, feature_id);
        fea_max = std::max(fea_max, feature_id+1);
      }
    } else if (type == FInfo::SPARSE) {
      // format: feature_id:value feature_id:value ...
      for (size_t j = 1; j < item.size(); ++j) {
        uint64 feature_id;
        float value;
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
        fg->add_feature_id(feature_id);
        if (!strtofloat(fea[1], &value)) {
          LL << "fail to parse value: " << fea[1];
          goto PARSE_PS_ERROR;
        }
        fg->add_value(value);
        fea_min = std::min(fea_min, feature_id);
        fea_max = std::max(fea_max, feature_id+1);
      }
    } else if (type == FInfo::DENSE) {
      // format: :value value ...
      for (size_t j = 1; j < item.size(); ++j) {
        float value;
        if (!strtofloat(item[j], &value)) {
          LL << "fail to parse value: " << item[j];
          goto PARSE_PS_ERROR;
        }
        fg->add_value(value);
      }
      fea_min = 0;
      fea_max = std::max(fea_max, (uint64)item.size());
    }
    auto& ginfo = group_info_[group_id];
    ginfo.set_feature_begin(std::min(ginfo.feature_begin(), fea_min));
    ginfo.set_feature_end(std::max(ginfo.feature_end(), fea_max));
    ginfo.set_num_entries(ginfo.num_entries() + item.size() - 1);
    // LL << group_info_[group_id].DebugString();
  }

  // write into file
  return record_writer_->WriteProtocolMessage(instance);

PARSE_PS_ERROR:
  LL << "failed to parse line " << num_line_processed_
     << " of " << FLAGS_input << ": " << line;
  return false;
}


bool Text2Pb::detectPServerInstance(const string& line) {
  auto group = split(line, ';', false);
  if (group.size() < 2) return false;

  // parse label
  float label;
  if (!detectLabel(group[0], &label)) return false;

  // parse feature group
  for (int i = 1; i < group.size(); ++i) {
    auto item = split(group[i], ' ', true);
    if (item.size() < 2) return false;

    int group_id;
    if (!strtoi32(item[0], &group_id)) return false;
    // LL << group_id;
    auto& info = group_info_[group_id];
    info.set_group_id(group_id);
    info.set_feature_end(0);
    info.set_feature_begin(kuint64max);
    for (int j = 1; j < item.size(); ++j) {
      typedef FeatureGroupInfo FInfo;
      uint64 feature_id;
      auto fea = split(item[j], ':', true);
      if (fea.size() == 2) {
        info.set_feature_type(FInfo::SPARSE);
      } else if (fea.size() == 1) {
        if (strtou64(item[j], &feature_id)) {
          if (!info.has_feature_type()) {
            info.set_feature_type(FInfo::SPARSE_BINARY);
          }
        } else {
          info.set_feature_type(FInfo::DENSE);
        }
      } else {
        return false;
      }
    }
  }
  return true;
}

void Text2Pb::processPServer(char *line) {
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

} // namespace PS

int main(int argc, char *argv[]) {
  using namespace PS;
  FLAGS_logtostderr = 1;
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  Text2Pb t2p;
  t2p.init();

  if (FLAGS_verbose) {
    std::cout << "read " << FLAGS_input << " in format " << FLAGS_text_format << std::endl;
  }

  FileLineReader reader(FLAGS_input.c_str());

  if (t2p.pserver())
    reader.set_line_callback([&t2p] (char *line) { t2p.processPServer(line); });
  else if (t2p.libsvm())
    reader.set_line_callback([&t2p] (char *line) { t2p.processLibSvm(line); });
  else if (t2p.vw())
    reader.set_line_callback([&t2p] (char *line) { t2p.processVW(line); });
  else
    CHECK(false) << "unknow text format " << FLAGS_text_format;

  reader.Reload();

  t2p.finalize();

  return 0;
}
