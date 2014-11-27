#include "data/info_parser.h"
namespace PS {

void InfoParser::clear() {
  for (int i = 0; i < kSlotIDmax; ++i) slot_info_[i].Clear();
  info_.Clear();
  num_ex_ = 0;
}

bool InfoParser::add(const Example& ex) {
  for (int i = 0; i < ex.slot_size(); ++i) {
    const auto& slot = ex.slot(i);
    if (slot.id() >= kSlotIDmax) return false;
    auto& sinfo = slot_info_[slot.id()];
    for (int j = 0; j < slot.key_size(); ++j) {
      uint64 key = slot.key(j);
      sinfo.set_min_key(std::min((uint64)sinfo.min_key(), key));
      sinfo.set_max_key(std::max((uint64)sinfo.max_key(), key + 1));
    }
    if (slot.key_size() > 0) {
      if (slot.val_size() == slot.key_size()) {
        sinfo.set_format(SlotInfo::SPARSE);
      } else {
        sinfo.set_format(SlotInfo::SPARSE_BINARY);
      }
    } else if (slot.val_size() > 0) {
      sinfo.set_format(SlotInfo::DENSE);
    }
    sinfo.set_nnz_ex(sinfo.nnz_ex() + 1);
    sinfo.set_nnz_ele(sinfo.nnz_ele() + std::max(slot.key_size(), slot.val_size()));
  }
  ++ num_ex_;
  return true;
}

ExampleInfo InfoParser::info() {
  info_.set_num_ex(num_ex_);
  info_.clear_slot();
  for (int i = 0; i < kSlotIDmax; ++i) {
    auto &sinfo = slot_info_[i];
    if (!sinfo.nnz_ele()) continue;
    sinfo.set_id(i);
    if (i == 0) {  // the label
      sinfo.set_min_key(0);
      sinfo.set_max_key(1);
    }
    *info_.add_slot() = sinfo;
  }
  return info_;
}
} // namespace PS
