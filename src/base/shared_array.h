#pragma once

#include "util/common.h"
#include "util/file.h"
#include "base/range.h"
#include "Eigen/Core"
// zlib is too slow
// #include "zlib.h"
#include "snappy.h"

namespace PS {

template<typename V> class Matrix;
template<typename V> class SArray;
template<typename V> using SArrayList = std::vector<SArray<V>>;

// TODO add capacity
template<typename V>
class SArray {
 public:
  SArray() { }
  // TODO move ctor

  // allocate
  explicit SArray(size_t n) { resize(n); }

  void resize(size_t n);

  // zero-copy, namely just copy pointer
  template <typename W> explicit SArray(const SArray<W>& arr);
  template <typename W> void operator=(const SArray<W>& arr);
  void reset(V* data, size_t size);

  // copy
  template <typename FwdForwardIt>
  SArray(const FwdForwardIt first, const FwdForwardIt last) {
    copyFrom(first, last);
  }
  SArray(const V* src, size_t size) {
    resize(size);
    memcpy(data_, src, size*sizeof(V));
  }

  template <typename W>
  SArray(const std::initializer_list<W>& list) {
    copyFrom(list.begin(), list.end());
  }
  template <typename W> void operator=(const std::initializer_list<W>& list) {
    copyFrom(list.begin(), list.end());
  }
  // might slow but general version
  template <typename ForwardIt>
  void copyFrom(const ForwardIt first, const ForwardIt last);
  // might faster than above
  void copyFrom(const SArray<V>& arr) {
    resize(arr.size());
    memcpy(data_, arr.data(), size()*sizeof(V));
  }

  void append(const SArray<V>& arr) {
    auto orig_size = size_;
    resize(size_ + arr.size());
    memcpy(data_+orig_size, arr.data(), arr.size()*sizeof(V));
  }

  // a segment start from pos with length len, zero-copy
  SArray<V> segment(size_t pos, size_t len) const;
  SArray<V> segment(SizeR range) const {
    CHECK(range.valid());
    return segment(range.begin(), range.size());
  }

  // intersection, union, find value range. assume values are ordered
  SArray<V> setIntersection(const SArray<V>& other);
  SArray<V> setUnion(const SArray<V>& other);

  // find the segment whose entry value are within [lower_bound, upper_bound)
  SizeR findRange (const V& lower_bound, const V& upper_bound) const;
  SizeR findRange (const Range<V>& range) const {
    // CHECK(range.valid());
    return findRange(range.begin(), range.end());
  }

  // accessor and mutator
  size_t size() const { return size_; }
  bool empty() const { return size() == 0; }

  V* data() const { return data_; }

  V* begin() { return data(); }
  const V* begin() const { return data(); }
  V* end() { return data() + size(); }
  const V* end() const { return data() + size(); }

  V back() const { return data_[size_-1]; }
  V front() const { return data_[0]; }

  Range<V> range() const { return Range<V>(front(), back()+1); }
  const shared_ptr<void>& ptr() const { return ptr_; }

  V& operator[] (int i) { return data_[i]; }
  const V& operator[] (int i) const { return data_[i]; }

  template <typename W> bool operator==(const SArray<W> &rhs) const;

  // map to an eigen vector
  typedef Eigen::Map<Eigen::Matrix<V, Eigen::Dynamic, 1> > EVecMap;
  EVecMap vec() const { return EVecMap(data(), size()); }

  // map to an eigen array
  typedef Eigen::Map<Eigen::Array<V, Eigen::Dynamic, 1> > EArrayMap;
  EArrayMap array() const { return EArrayMap(data(), size()); }

  // map to an dense matrix
  shared_ptr<Matrix<V>> matrix(size_t rows = -1, size_t cols = -1);

  size_t nnz() const {
    size_t ret = 0;
    for (size_t i = 0; i < size_; ++i) ret += data_[i] == 0 ? 0 : 1;
    return ret;
  }

  void setZero() { memset(data_, 0, size_*sizeof(V)); }

  void plus(const SArray<V>& other);


  // by snappy
  SArray<char> compressTo() const;

  // allocate enough space first
  void uncompressFrom(const char* src, size_t src_size);

  // I/O read and write binary files
  bool readFromFile(const string& file_name) {
    return readFromFile(SizeR::all(), file_name);
  }
  bool readFromFile(size_t begin, size_t length, const string& file_name) {
    return readFromFile(SizeR(begin, begin+length), file_name);
  }

  bool readFromFile(SizeR range, const string& file_name);

  bool writeToFile(const string& file_name) const {
    return writeToFile(SizeR::all(), file_name);
  }

  bool writeToFile(SizeR range, const string& file_name) const;


 private:

  size_t size_ = 0;
  V* data_ = nullptr;
  shared_ptr<void> ptr_ = shared_ptr<void>(nullptr);
};

template <typename V>
void SArray<V>::resize(size_t n) {
  if (size_ >= n) { size_ = n; return; }
  V* data = new V[n+5];
  memcpy(data, data_, size_*sizeof(V));
  reset(data, n);
}

template <typename V>
template <typename ForwardIt>
void SArray<V>::copyFrom(const ForwardIt first, const ForwardIt last) {
  size_ = std::distance(first, last);
  data_ = new V[size_+5];
  ptr_.reset(reinterpret_cast<char*>(data_), [](char *p) { delete [] p; });
  for (size_t i = 0; i < size_; ++i)
    data_[i] = *(first+i);
}

template <typename V>
void SArray<V>::reset(V* data, size_t size) {
  size_ = size;
  data_ = data;
  ptr_.reset(reinterpret_cast<char*>(data_), [](char *p) { delete [] p; });
}

template <typename V>
template <typename W>
SArray<V>::SArray(const SArray<W>& arr) {
  *this = arr;
  // size_ = arr.size() * sizeof(W) / sizeof(V);
  // data_ = reinterpret_cast<V*>(arr.data());
  // ptr_ = arr.ptr();
}

template <typename V>
template <typename W>
void SArray<V>::operator=(const SArray<W>& arr) {
  size_ = arr.size() * sizeof(W) / sizeof(V);
  data_ = reinterpret_cast<V*>(arr.data());
  ptr_ = arr.ptr();
}

template <typename V>
SArray<V> SArray<V>::segment(size_t pos, size_t len) const {
  CHECK_LE(pos+len, size());
  SArray<V> result = *this;
  result.data_ += pos;
  result.size_ = len;
  return result;
}

template <typename V>
template <typename W>
bool SArray<V>::operator==(const SArray<W> &rhs) const {
  if (rhs.size() * sizeof(W) != size() * sizeof(V))
    return false;
  if (size() == 0)
    return true;
  return (memcmp(data(), rhs.data(), size() * sizeof(V)) == 0);
}

template <typename V>
std::ostream& operator<<(std::ostream& os, const SArray<V>& obj) {
  os << dbstr(obj.data(), obj.size(), 10);
  return os;
}

template <typename V>
SArray<V> SArray<V>::setIntersection(const SArray<V>& other) {
  SArray<V> result(std::min(other.size(), size())+1);
  V* last = std::set_intersection(
      begin(), end(), other.begin(), other.end(), result.begin());
  result.size_ = last - result.begin();
  return result;
}

template <typename V>
SArray<V> SArray<V>::setUnion(const SArray<V>& other) {
  SArray<V> result(other.size() + size());
  V* last = std::set_union(
      begin(), end(), other.begin(), other.end(), result.begin());
  result.size_ = last - result.begin();
  return result;
}

template <typename V>
SizeR SArray<V>::findRange (
    const V& lower_bound, const V& upper_bound) const {
  // if (upper_bound <= lower_bound)
  //   return SizeR(0,0);
  auto lb = std::lower_bound(begin(), end(), lower_bound);
  auto ub = std::lower_bound(begin(), end(), upper_bound);
  return SizeR(lb - begin(), ub - begin());
}

template <typename V>
void SArray<V>::plus(const SArray<V>& other) {
  CHECK_EQ(size(), other.size());
  for (int i = 0; i < size(); ++i) {
    data_[i] += other.data_[i];
  }
}

// template <typename V>
// double SArray<V>::norm(p) {

// }

  // zlib version
  // unsigned long dsize = size_ + (size_ * 0.1f) + 16;  // NOLINT
  // SArray<char> dest(dsize);
  // // Use compress() from zlib.h.
  // int ret =
  //     compress(reinterpret_cast<unsigned char*>(dest.data()), &dsize,
  //              reinterpret_cast<const unsigned char*>(data_), size_);
  // CHECK_EQ(ret, Z_OK) << "Compress error occured! Error code: " << ret;
  // dest.resize(dsize);
  // return dest;

template <typename V>
SArray<char> SArray<V>::compressTo() const {
  size_t dsize = snappy::MaxCompressedLength(size_);
  SArray<char> dest(dsize);
  snappy::RawCompress(
      reinterpret_cast<const char*>(data_), size_, dest.data(), &dsize);
  dest.resize(dsize);
  return dest;
}


} // namespace PS
