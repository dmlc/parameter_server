#pragma once
#include "util/common.h"
#include "proto/example.pb.h"
#include "proto/config.pb.h"
namespace PS {

// parse an example from various text formats into the protobuf format,
// e.g. proto/example.proto
class ExampleParser {
 public:
  typedef DataConfig::TextFormat TextFormat;
  void init(TextFormat format, bool ignore_fea_slot = false);
  bool toProto(char*, Example*);
 private:
  bool parseLibsvm(char*,  Example*);
  bool parseAdfea(char*,  Example*);
  bool parseTerafea(char*,  Example*);
  std::function<bool(char*, Example*)> parser_;
  bool ignore_fea_slot_;
};

}
