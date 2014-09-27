#pragma once

#include <math.h>
#include "base/shared_array_inl.h"
namespace PS {

// may support template key
class CountMin {
 public:
  // TODO prefetch to accelerate the memory access
  bool empty() { return n_ == 0; }
  void clear() { data_.clear(); n_ = 0; }
  void resize(int n, int k) {
    n_ = std::max(n, 64);
    data_.resize(n_);
    data_.setZero();
    k_ = std::min(30, std::max(1, k));
  }

  void bulkInsert(const SArray<uint64>& key, const SArray<uint32>& count) {
    CHECK_GT(n_, 0);
    CHECK_EQ(key.size(), count.size());
    for (size_t i = 0; i < key.size(); ++i) {
      uint32 h = hash(key[i]);
      const uint32 delta = (h >> 17) | (h << 15);  // Rotate right 17 bits
      for (int j = 0; j < k_; ++j) {
        data_[h % n_] += count[i];
        h += delta;
      }
    }

  }

  uint32 query(const uint64& key) const {
    uint32 res = kuint32max;
    uint32 h = hash(key);
    const uint32 delta = (h >> 17) | (h << 15);  // Rotate right 17 bits
    for (int j = 0; j < k_; ++j) {
      res = std::min(res, data_[h % n_]);
      h += delta;
    }
    return res;
  }

 private:
  uint32 hash(const uint64& key) const {
    // similar to murmurhash
    const uint32 seed = 0xbc9f1d34;
    const uint32 m = 0xc6a4a793;
    const uint32 n = 8;  // sizeof uint64
    uint32 h = seed ^ (n * m);

    uint32 w = (uint32) key;
    h += w; h *= m; h ^= (h >> 16);

    w = (uint32) (key >> 32);
    h += w; h *= m; h ^= (h >> 16);
    return h;
  }

  SArray<uint32> data_;
  int n_ = 0;
  int k_ = 1;
};

} // namespace PS
