#pragma once
#include "base/sketch.h"
namespace PS {

// a blocked version, see
// Cache-, Hash- and Space-Efficient Bloom Filters,
// http://algo2.iti.kit.edu/documents/cacheefficientbloomfilters-jea.pdf

// 1.2x - 1.8x faster than BloomFilter, but may give slightly large FPR
template <typename K>
class BlockBloomFilter : public Sketch {
 public:
  BlockBloomFilter() { }
  BlockBloomFilter(int m, int k) { resize(m, k); }
  ~BlockBloomFilter() { delete [] data_; }
  void resize(int m, int k) {
    delete [] data_;
    k_ = std::min(64, std::max(1, k));
    m_ = m;
    num_bin_ = (m / 8 / bin_size_) + 1;
    data_size_ = num_bin_ * bin_size_;
    data_ = new char[data_size_];
    // CHECK_EQ(posix_memalign((void**)&data_, bin_size_*8, data_size_), 0);
    memset(data_, 0, data_size_ * sizeof(char));
  }

  bool operator[] (K key) const { return query(key); }
  bool query(K key) const {
    uint32 h = hash(key);
    const uint32 delta = (h >> 17) | (h << 15);  // Rotate right 17 bits
    char* data = data_ + (h % num_bin_) * bin_size_;
    for (int j = 0; j < k_; ++j) {
      uint32 bitpos = h % (bin_size_ * 8);
      if ((data[bitpos/8] & (1 << (bitpos % 8))) == 0) return false;
      h += delta;
    }
    return true;
  }

  void insert(K key) {
    uint32 h = hash(key);
    const uint32 delta = (h >> 17) | (h << 15);  // Rotate right 17 bits
    char* data = data_ + (h % num_bin_) * bin_size_;
    for (int j = 0; j < k_; ++j) {
      uint32 bitpos = h % (bin_size_ * 8);
      data[bitpos/8] |= (1 << (bitpos % 8));
      h += delta;
    }
  }

 private:
  char* data_ = NULL;
  int data_size_ = 0;
  uint32 m_ = 0;
  int k_ = 0;
  const uint32 bin_size_ = 64;  // cache line size
  uint32 num_bin_ = 0;
};

}
