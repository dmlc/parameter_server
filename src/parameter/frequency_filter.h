#pragma once
#include "base/countmin.h"
#include "system/postoffice.h"
#include "base/shared_array_inl.h"

namespace PS {

template<typename K, typename V>
class FreqencyFilter {
 public:
  // add unique keys with their key count
  void insertKeys(const SArray<K>& key, const SArray<V>& count);
  // filter keys using the threadhold *freqency*
  SArray<K> queryKeys(const SArray<K>& key, int freqency);

  bool empty() { return count_.empty(); }
  void resize(int n, int k) { count_.resize(n, k, 254); }
  void clear() { count_.clear(); }

 private:
  CountMin<K, V> count_;
};

// countmin implementation
template<typename K, typename V>
SArray<K> FreqencyFilter<K,V>::queryKeys(const SArray<K>& key, int freqency) {
  CHECK_LT(freqency, kuint8max) << "change to uint16 or uint32...";
  SArray<K> filtered_key;
  for (auto k : key) {
    if (count_.query(k) > freqency) {
     filtered_key.pushBack(k);
    }
  }
  return filtered_key;
}

template<typename K, typename V>
void FreqencyFilter<K,V>::insertKeys(const SArray<K>& key, const SArray<V>& count) {
  CHECK_EQ(key.size(), count.size());
  for (size_t i = 0; i < key.size(); ++i) {
    count_.insert(key[i], count[i]);
  }
}

// hash implementation
// std::unordered_map<K, V> map_;

// template<typename K>
// SArray<K> FreqencyFilter<K>::queryKeys(const SArray<K>& key, int freqency) {
//   SArray<K> filtered_key;
//   for (K k : key) {
//     if (map_[k] > freqency) filtered_key.pushBack(k);
//   }
//   return filtered_key;
// }

// template<typename K>
// void FreqencyFilter<K>::insertKeys(const SArray<K>& key, const SArray<uint32>& count) {
//   CHECK_EQ(key.size(), count.size());
//   for (size_t i = 0; i < key.size(); ++i) {
//     map_[key[i]] += count[i];
//   }
// }

}
