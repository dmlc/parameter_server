#pragma once
#include "util/common.h"
#include "proto/example.pb.h"
#include "proto/config.pb.h"

namespace PS {

static const int kSlotIDmax = 4096;

class ExampleParser {
 public:
  typedef DataConfig::TextFormat TextFormat;
  void init(TextFormat format, bool ignore_fea_slot = false);
  void clear();

  bool toProto(char*, Example*);
  ExampleInfo info();

  int maxSlotID() { return ignore_fea_slot_ ? 2 : kSlotIDmax; }
 private:
  bool parseLibsvm(char*,  Example*);
  bool parseAdfea(char*,  Example*);
  bool parseTerafea(char*,  Example*);

  ExampleInfo info_;

  SlotInfo slot_info_[kSlotIDmax];
  bool ignore_fea_slot_;
  size_t num_ex_ = 0;
  TextFormat format_;

  std::function<bool(char*, Example*)> parser_;
};

}
