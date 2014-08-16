#include "util/common.h"
#include "util/split.h"
#include "util/filelinereader.h"
#include "util/recordio.h"
#include "util/strtonum.h"
#include "proto/instance.pb.h"

namespace PS {

// convert text format input into binary protobuf format
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
// adfea: sparse
// line_id 1 click fea_id:group_id fea_id:group_id ...

class Text2Proto {
 public:
  void init();

  void write();

  void processPServer(char *line);
  void processLibSvm(char *line);
  void processAdFea(char *line);

  void processVW(char *line) { } // TODO

  bool pserver() { return format_ == "pserver"; }
  bool libsvm() { return format_ == "libsvm"; }
  bool vw() { return format_ == "vw"; }
  bool adfea() { return format_ == "adfea"; }

  size_t num_lines_written() { return records_.size(); }
  size_t num_lines_skipped() { return num_processed_ - num_lines_written(); }

  // string text_format() { return text_format_; }
 private:

  bool parsePServerInstance(const string& line);
  bool detectPServerInstance(const string& line);
  void writeInfo();

  bool parseLabel(const string& label_str, float* label);
  bool detectLabel(const string& label_str, float* label);

  string format_;
  unique_ptr<RecordWriter> writer_ = nullptr;

  size_t num_processed_ = 0;

  InstanceInfo info_;
  std::map<int, FeatureGroupInfo> group_info_;
  // size_t info_size_ = 0;

  // used to detect format
  std::vector<string> file_header_;

  std::vector<Instance> records_;
};

} // namespace PS
