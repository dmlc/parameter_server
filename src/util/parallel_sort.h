#pragma once

#include "base/shared_array.h"

namespace PS {

template<typename T> void parallelSort(T* data, size_t len, size_t grainsize) {
  if (len <= grainsize) {
    // Timer t; t.start();
    std::sort(data, data + len, std::less<T>());
    // LL << len << " " << t.get();
  } else {
    std::thread thr(parallelSort<T>, data, len/2, grainsize);
    parallelSort(data + len/2, len - len/2, grainsize);
    thr.join();

    // Timer t; t.start();
    std::inplace_merge(data, data + len/2, data + len, std::less<T>());
    // LL << len << " " << t.get();
  }
}

template<typename T> void parallelSort(SArray<T>* arr, int num_threads) {
  CHECK_GT(num_threads, 0);
  size_t grainsize = std::max(arr->size() / num_threads + 5, (size_t)256);
  parallelSort(arr->data(), arr->size(), grainsize);
}

} // namespace PS
