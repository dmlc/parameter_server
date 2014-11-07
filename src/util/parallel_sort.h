#pragma once

#include "base/shared_array.h"

namespace PS {
template<typename T, class Fn>
void parallelSort(T* data, size_t len, size_t grainsize, const Fn& cmp) {
  if (len <= grainsize) {
    std::sort(data, data + len, cmp);
  } else {
    std::thread thr(parallelSort<T, Fn>, data, len/2, grainsize, cmp);
    parallelSort(data + len/2, len - len/2, grainsize, cmp);
    thr.join();

    std::inplace_merge(data, data + len/2, data + len, cmp);
  }
}

// cmp typically is [](const T& a, const T& b) { return a < b; }
// or an even simplier version: std::less<T>()
template<typename T, class Fn>
void parallelSort(SArray<T>* arr, int num_threads, const Fn& cmp) {
  CHECK_GT(num_threads, 0);
  size_t grainsize = std::max(arr->size() / num_threads + 5, (size_t)1024*16);
  parallelSort(arr->data(), arr->size(), grainsize, cmp);
}

} // namespace PS
