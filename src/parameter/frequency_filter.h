#pragma once
#include "util/countmin.h"
#include "system/postoffice.h"

namespace PS {

template<typename K>
class FreqencyFilter {
 public:
  // add unique keys with their key count
  void addKeys(const SArray<K>& key, const SArray<uint32>& count) {
    if (key.empty()) return;
    if (count_.capacity() == 0) {
      count_.resize(key.size() * FLAGS_num_workers * 10, 2);
    }
    count_.bulkInsert(key, count);
  }

  // filter keys using the threadhold *freqency*
  SArray<K> filterKeys(const SArray<K>& key, int freqency) const {
    SArray<K> filtered_key;
    for (auto k : key) {
      if (count_.query(k) > freqency) filtered_key.pushBack(k);
    }
    return filtered_key;
  }
 private:
  CountMin count_;
};

}
