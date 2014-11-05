#pragma once
#include "util/common.h"
// #include "base/shared_array_inl.h"
namespace PS {
namespace PARSA {

// double linked list supporting random access and cache, the value is sorted
class DblinkArray {
 public:
  void init(const std::vector<int>& data, int cache_limit);
  void remove(int i);
  void decrAndSort(int i);
  int minIdx() { return cached_pos_[0]; }
 private:
  struct Entry {
    int value;
    int prev = -1;
    int next = -1;
  };
  std::vector<Entry> data_;
  // cached_pos_[i] is the first index whose value >= i
  std::vector<int> cached_pos_;
  int cache_limit_;
};

} // namespace PARSA
} // namespace PS
