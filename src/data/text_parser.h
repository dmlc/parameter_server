#pragma once
#include "util/common.h"
#include "data/proto/example.pb.h"
#include "data/proto/data.pb.h"
namespace PS {

// parse an example from various text formats into the protobuf format,
// e.g. proto/example.proto
class ExampleParser {
 public:
  typedef DataConfig::TextFormat TextFormat;
  void Init(TextFormat format, bool ignore_fea_slot = false);
  bool ToProto(char*, Example*);
 private:
  bool ParseLibsvm(char*,  Example*);
  bool ParseAdfea(char*,  Example*);
  bool ParseTerafea(char*,  Example*);
  bool ParsePS(char*, Example*, TextFormat);
  bool ParseCriteo(char*,  Example*);
  std::function<bool(char*, Example*)> parser_;
  bool ignore_fea_slot_;
};

}
