#pragma once
#include "util/common.h"
#include "util/range.h"
#include "util/rawarray.h"
namespace PS {

// TODO support more key types, such as 32bit 48bit 128bit
typedef uint64 key_t;
typedef uint64 Key;
typedef uint32 MatrixKey;
typedef Range<Key> KeyRange;
static const Key kMaxKey = kuint64max;

// slice keys by the range
RawArray Slice(const RawArray& keys, const KeyRange& range);

// slice values by the provides keys
RawArray Slice(const RawArray& keys, const RawArray& values,
               const RawArray& slice_keys);

// assume data are ordered by non-decreasing. return the largest index whose
// value <= val, return -1 if val is less than data[0]
// TODO use binary search... refer to ../../back/common.h
template<class T>
int64 LowerBound(const T*data, size_t length, T val) {
  for (int64 i = 0; i < length; ++i) {
    if (data[i] == val) return i;
    if (data[i] > val) return i - 1;
  }
  return length - 1;
}
// assume data are ordered by non-decreasing. return the least index whose
// value >= val, return length if val is greater than data[length-1]
template<class T>
int64 UpperBound(const T*data, size_t length, T val) {
  for (int64 i = length-1; i >= 0; --i) {
    if (data[i] == val) return i;
    if (data[i] < val ) return i + 1;
  }
  return 0;
}

} // namespace PS
