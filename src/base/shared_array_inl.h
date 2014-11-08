#pragma once
#include "base/shared_array.h"
#include "base/dense_matrix.h"
#include <random>
#include "snappy.h"

namespace PS {

template <typename V>
void SArray<V>::resize(size_t n) {
  if (capacity_ >= n) { size_ = n; return; }
  V* data = new V[n+5];
  memcpy(data, data_, size_*sizeof(V));
  reset(data, n);
}

template <typename V>
void SArray<V>::reserve(size_t n) {
  if (capacity_ >= n) { return; }
  size_t old_size = size_;
  resize(n);
  size_ = old_size;
}

template <typename V>
template <typename ForwardIt>
void SArray<V>::copyFrom(const ForwardIt first, const ForwardIt last) {
  size_t size = std::distance(first, last);
  V* data = new V[size + 5];
  reset(data, size);
  for (size_t i = 0; i < size_; ++i) {
    data_[i] = *(first+i);
  }
}

template <typename V>
void SArray<V>::copyFrom(const SArray<V>& arr) {
  copyFrom(arr.data(), arr.size());
}

template <typename V>
template <typename W>
SArray<V>::SArray(const std::initializer_list<W>& list) {
  copyFrom(list.begin(), list.end());
}

template <typename V>
template <typename W>
void SArray<V>::operator=(const std::initializer_list<W>& list) {
  copyFrom(list.begin(), list.end());
}

template <typename V>
void SArray<V>::reset(V* data, size_t size) {
  capacity_ = size;
  size_ = size;
  data_ = data;
  ptr_.reset(reinterpret_cast<char*>(data_), [](char *p) {
      delete [] p;
    });
}

template <typename V>
template <typename W>
SArray<V>::SArray(const SArray<W>& arr) {
  *this = arr;
}

template <typename V>
template <typename W>
void SArray<V>::operator=(const SArray<W>& arr) {
  size_ = arr.size() * sizeof(W) / sizeof(V);
  capacity_ = arr.capacity() * sizeof(W) / sizeof(V);
  data_ = reinterpret_cast<V*>(arr.data());
  ptr_ = arr.pointer();
}

template <typename V>
template <typename W>
bool SArray<V>::operator==(const SArray<W> &rhs) const {
  if (rhs.size() * sizeof(W) != size() * sizeof(V)) return false;
  if (size() == 0) return true;
  return (memcmp(data(), rhs.data(), size() * sizeof(V)) == 0);
}

template <typename V>
void SArray<V>::pushBack(const V& val) {
  if (size_ == capacity_) reserve(size_*2+5);
  data_[size_++] = val;
}

template <typename V>
size_t SArray<V>::nnz() const {
  size_t ret = 0;
  for (size_t i = 0; i < size(); ++i) ret += data_[i] == 0 ? 0 : 1;
  return ret;
}

template <typename V>
void SArray<V>::setValue(V value) {
  if (value == 0) {
    setZero();
  } else {
    for (size_t i = 0; i < size_; ++i) data_[i] = value;
  }
}

template <typename V>
void SArray<V>::setValue(const ParameterInitConfig& cf) {
  typedef ParameterInitConfig Type;
  if (cf.type() == Type::ZERO) {
    setZero();
  } else if (cf.type() == Type::CONSTANT) {
    setValue((V)cf.constant());
  } else if (cf.type() == Type::GAUSSIAN) {
    for (size_t i = 0; i < size_; ++i) {
      std::default_random_engine generator;
      std::normal_distribution<V> distribution((V)cf.mean(), (V)cf.std());
      data_[i] = distribution(generator);
    }
  } else if (cf.type() == Type::FILE) {
    CHECK(false);
    // TODO read from file
    // size_t n = File::size(cf.file_name());
  }
}

template <typename V>
SArray<V> SArray<V>::segment(const Range<size_t>& range) const {
  CHECK(range.valid());
  CHECK_LE(range.end(), size());
  SArray<V> result = *this;
  result.data_ += range.begin();
  result.size_ = range.size();
  result.capacity_ = range.size();
  return result;
}

template <typename V>
void SArray<V>::copyFrom(const V* src, size_t size) {
  resize(size);
  memcpy(data_, src, size*sizeof(V));
}

template <typename V>
SArray<V> SArray<V>::setIntersection(const SArray<V>& other) const {
  SArray<V> result(std::min(other.size(), size())+1);
  V* last = std::set_intersection(
      begin(), end(), other.begin(), other.end(), result.begin());
  result.size_ = last - result.begin();
  result.capacity_ = result.size_;
  return result;
}

template <typename V>
SArray<V> SArray<V>::setUnion(const SArray<V>& other) const {
  SArray<V> result(other.size() + size());
  V* last = std::set_union(
      begin(), end(), other.begin(), other.end(), result.begin());
  result.size_ = last - result.begin();
  return result;
}

template <typename V>
SizeR SArray<V>::findRange (const Range<V>& bound) const {
  if (empty()) return SizeR(0,0);
  CHECK(bound.valid());
  auto lb = std::lower_bound(begin(), end(), bound.begin());
  auto ub = std::lower_bound(begin(), end(), bound.end());
  return SizeR(lb - begin(), ub - begin());
}

template <typename V>
bool SArray<V>::readFromFile(SizeR range, const string& file_name) {
  DataConfig data;
  data.set_format(DataConfig::BIN);
  data.add_file(file_name);
  return readFromFile(range, data);
}

template <typename V>
MatrixPtr<V> SArray<V>::matrix(size_t rows, size_t cols) {
  // TODO rows and cols
  MatrixInfo info;
  info.set_type(MatrixInfo::DENSE);
  info.set_row_major(false);
  SizeR(0, size_).to(info.mutable_row());
  SizeR(0,1).to(info.mutable_col());
  info.set_nnz(size_);
  info.set_sizeof_value(sizeof(V));
  return MatrixPtr<V>(new DenseMatrix<V>(info, *this));
}

template <typename V>
bool SArray<V>::readFromFile(SizeR range, const DataConfig& data) {
  if (range == SizeR::all()) {
    range = SizeR(0, File::size(data.file(0))/sizeof(V));
  }
  if (range.empty()) { clear(); return true; }

  File* file = File::open(data, "r");
  if (file == NULL || !file->open()) return false;
  resize(range.size());
  if (range.begin() > 0) file->seek(range.begin() * sizeof(V));
  size_t length = range.size() * sizeof(V);
  return (file->read(ptr_.get(), length) == length && file->close());
}

template <typename V>
bool SArray<V>::writeToFile(SizeR range, const string& file_name) const {
  if (range.empty()) return true;
  CHECK(range.valid());
  CHECK_LE(range.end(), size_);

  File* file = File::open(file_name, "w");
  size_t length = range.size() * sizeof(V);
  return (file->write(ptr_.get(), length) == length
          && file->flush() && file->close());
}

template <typename V>
void SArray<V>::uncompressFrom(const char* src, size_t src_size) {
  if (src_size == 0) { clear(); return; }
  size_t dsize = 0;
  CHECK(snappy::GetUncompressedLength(src, src_size, &dsize));
  CHECK_EQ(dsize/sizeof(V)*sizeof(V), dsize);
  resize(dsize/sizeof(V));
  // CHECK_LE(dsize, size_);
  CHECK(snappy::RawUncompress(src, src_size, reinterpret_cast<char*>(data_)));
}


template <typename V>
SArray<char> SArray<V>::compressTo() const {
  // otherwise, snappy will add a 0 here...
  if (empty()) return SArray<char>();
  size_t ssize = size_ * sizeof(V);
  size_t dsize = snappy::MaxCompressedLength(ssize);
  SArray<char> dest(dsize);
  snappy::RawCompress(
      reinterpret_cast<const char*>(data_), ssize, dest.data(), &dsize);
  dest.resize(dsize);
  return dest;
}

template <typename V>
void SArray<V>::append(const SArray<V>& arr) {
  if (arr.empty()) return;
  auto orig_size = size_;
  resize(size_ + arr.size());
  memcpy(data_+orig_size, arr.data(), arr.size()*sizeof(V));
}

} // namespace PS

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
