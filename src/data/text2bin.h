#include "util/common.h"
#include "util/split.h"
#include "util/filelinereader.h"
#include "util/recordio.h"
#include "util/strtonum.h"
#include "proto/instance.pb.h"

namespace PS {

// convert text format input into binary or protobuf format
//
// pserver format:
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
// libsvm:
//
// label feature_id:weight feature_id:weight feature_id:weight ...
//
// vw: TODO
//
// adfea:
// line_id 1 click fea_id:group_id fea_id:group_id ...

class Text2Bin {
 public:
  void init();

  void finalize();

  void processPServer(char *line);
  void processLibSvm(char *line);
  void processAdFea(char *line);

  // TODO
  void processVW(char *line) { }

  bool pserver() { return text_format_ == "pserver"; }
  bool libsvm() { return text_format_ == "libsvm"; }
  bool vw() { return text_format_ == "vw"; }
  bool adfea() { return text_format_ == "adfea"; }


  size_t num_lines_written() { return num_line_written_; }
  size_t num_lines_skipped() { return num_line_processed_ - num_line_written_; }

  // string text_format() { return text_format_; }
 private:

  bool parsePServerInstance(const string& line);
  bool detectPServerInstance(const string& line);
  void writeInfo();

  bool parseLabel(const string& label_str, float* label);
  bool detectLabel(const string& label_str, float* label);

  string text_format_;
  File* record_file_ = nullptr;
  // File* preview_file_ = nullptr;
  unique_ptr<RecordWriter> record_writer_ = nullptr;
  size_t num_line_processed_ = 0;
  size_t num_line_written_ = 0;

  InstanceInfo info_;
  std::map<int, FeatureGroupInfo> group_info_;
  // size_t info_size_ = 0;

  // used to detect format
  std::vector<string> file_header_;


};

} // namespace PS
