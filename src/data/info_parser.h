#pragma once
#include "util/common.h"
#include "proto/example.pb.h"
#include "proto/config.pb.h"
namespace PS {

// the maximal allowed slot id
static const int kSlotIDmax = 4096;

class InfoParser {
 public:
  // void init(const DataConfig& conf) { conf_ = conf; }
  bool add(const Example& ex);
  void clear();
  ExampleInfo info();
  // int maxSlotID() { return conf_.ignore_fea_slot() ? 2 : kSlotIDmax; }
 private:
  // DataConfig conf_;
  size_t num_ex_ = 0;
  ExampleInfo info_;
  SlotInfo slot_info_[kSlotIDmax];
};

} // namespace PS
