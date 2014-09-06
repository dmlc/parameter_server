#pragma once
#include "system/customer.h"
namespace PS {

template<typename K>
class FreqencyFilter {
 public:
  void addKeys(const SArray<K>& key, const SArray<uint32>& count) { }

  SArray<K> filterKeys(const SArray<K>& key, int freqency) { }

 private:
  // TODO implement by countmin to save memory
  SArray<K> key_;
  SArray<uint32> count_;
};
}
