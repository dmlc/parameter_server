#pragma once
#include "util/rawarray.h"
namespace PS {

// this class is a templated wrapper of RawArray faciliating read and write. It
// works like std::array. But there is one thing different: copying is by
// reference, NOT by value, due to the shared_ptr

template <typename T>
class XArray {
 public:
  // constructors
  XArray() : data_(NULL) { }
  XArray(size_t n);
  XArray(RawArray raw);
  XArray(std::initializer_list<T> list);
  // do not delete raw_, the shared_ptr in raw_ will take care of it
  ~XArray() { }

  // accessors and mutators
  size_t size() { return len_; }
  T* data() { return data_; }
  T& operator[] (const size_t i) { return data_[i]; }
  const T operator[] (const size_t i) const { return data_[i]; }
  RawArray& raw() { return raw_; }
  const RawArray& raw() const { return raw_; }
  void set(size_t i, T val) {
    data_[i] = val;
  }
  // assume data are ordered by non-decreasing. return the largest pointer whose
  // value <= val, return NULL if val is less than data[0]
  // TODO use binary search... refer to ../../back/common.h
  T* LowerBound(const T& val, size_t *idx = NULL);
  // assume data are ordered by non-decreasing. return the least pointer whose
  // value >= val, return NULL if val is greater than data[len_-1]
  T* UpperBound(const T& val, size_t *idx = NULL);

  string DebugString();
  bool Empty() { return data_ == NULL; }
  //TODO:
  void resetSize(size_t newSize);
 private:
  size_t len_;
  T *data_;
  RawArray raw_;
};
//Producing segfault: double free error. to be fixed.
template <typename T> void
XArray<T>::resetSize(size_t n){
  // FIXME
  CHECK(0);
	raw_.resetEntryNum(n);
	len_=n;
	if(n==0){return;}
//	T* newData=(T*)realloc(data_,n);
//	CHECK_NOTNULL(newData);
	data_=(T*)raw_.data();
}
template <typename T>
XArray<T>::XArray(size_t n) : len_(n) {
  data_ = new T[n];
  raw_.Fill((char*)data_, sizeof(T), n);
}

template <typename T>
XArray<T>::XArray(std::initializer_list<T> list) {
  len_ = list.size();
  data_ = new T[len_];
  auto d = data_;
  for (T v : list) { *d = v; ++d; }
  raw_.Fill((char*)data_, sizeof(T), len_);
}

template <typename T>
XArray<T>::XArray(RawArray raw) : raw_(raw) {
  len_ = raw.size() / sizeof(T);
  CHECK_EQ(len_*sizeof(T), raw.size());
  data_ = (T*) raw.data();
}

template <typename T>
string XArray<T>::DebugString() {
  string str = std::to_string(size()) + ":[";
  int n = std::min((int)size(), 2);
  for (int i = 0; i < n; ++i) {
    if (i < n - 1)
      str += std::to_string(data_[i]) + ", ";
    else
      str += std::to_string(data_[i]) + "]";
  }
  return str;
}

template<class T>
T* XArray<T>::LowerBound(const T& val, size_t *idx) {
  if (Empty() || data_[0] > val)
    return NULL;
  size_t i = 0;
  for (; i < len_; ++i) {
    if (data_[i] == val) {
      ++i;
      break;
    }
    if (data_[i] > val) {
      break;
    }
  }
  if (idx != NULL) *idx = i - 1;
  return data_ + i - 1;
}

template<class T>
T* XArray<T>::UpperBound(const T& val, size_t *idx) {
  if (Empty() || data_[len_-1] < val)
    return NULL;
  size_t i = len_;
  for (; i > 0; --i) {
    if (data_[i-1] == val) {
      -- i;
      break;
    }
    if (data_[i-1] < val ) {
      break;
    }
  }
  if (idx != NULL) *idx = i ;
  return data_ + i;
}
} // namespace PS
