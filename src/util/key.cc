#include "util/key.h"

namespace PS {


RawArray Slice(const RawArray& keys, const KeyRange& range) {
  CHECK_EQ(keys.entry_size(), sizeof(Key));
  CHECK(range.Valid());
  const Key* data = (Key*)keys.data();
  int64 a = UpperBound<Key>(data, keys.entry_num(), range.start());
  int64 b = LowerBound<Key>(data, keys.entry_num(), range.end()-1);
  if (b < a) return RawArray();
  size_t key_size = keys.entry_size();
  size_t size = (b-a+1) * key_size;
  char* dest = new char[size];
  memcpy(dest, data+a, size);
  return RawArray(dest, key_size, b-a+1);
}

RawArray Slice(const RawArray& keys, const RawArray& values,
               const RawArray& slice_keys) {
  CHECK_EQ(keys.entry_size(), sizeof(Key));

  if (slice_keys.empty()) return RawArray();
  CHECK_EQ(slice_keys.entry_size(), sizeof(Key));
  if (slice_keys.size() == keys.size()
      && slice_keys.cksum() == keys.cksum() )
    return values;
  // locate the start point
  const Key* k1 = (Key*)keys.data();
  const Key* k2 = (Key*)slice_keys.data();
  int64 num1 = keys.entry_num();
  int64 num2 = slice_keys.entry_num();
  int64 a = UpperBound<Key>(k1, num1, k2[0]);
  if (a < 0) return RawArray();
  // one-by-one compare
  int64 v_size = values.entry_size();
  const char* v1 = (char*) values.data() + a * v_size;
  // const char* v1o = v1;
  char* v2 = new char[v_size*num2];
  char* v2o = v2;
  int64 i1 = a, i2 = 0, n = 0;
  while (i1 < num1 && i2 < num2) {
    if (k1[i1] == k2[i2]) {
      ++i1; ++i2; ++ n;
      if (i1 < num1 && i2 < num2)
        continue;
    }
    // do copy if key are not matched or reach the end
    if (n > 0) {
      int64 s = n * v_size;
      memcpy(v2, v1, s);
      v2 += s; v1 += s;
      n = 0;
    } else if (k1[i1] > k2[i2]) {
      ++ i2;
    } else  {
      ++ i1; v1 += v_size;
    }
  }
  return RawArray(v2o, v_size, (v2-v2o)/v_size);
}


} // namespace PS
