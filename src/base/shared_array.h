#pragma once

#include "util/common.h"
#include "util/file.h"
#include "base/range.h"
#include "Eigen/Core"

namespace PS {

template<typename V> class Matrix;
template<typename V> class SArray;
template<typename V> using SArrayList = std::vector<SArray<V>>;

// Memory efficient array. Most operations are zero-copy, such as assign, slice
// a segment, convert to Eigen3 vector/array. It shares the same semantic as a C
// array pointer. For example,
//   SArray<int> A(10);
//   SArray<int> B = A;
//   SArray<int> C = A.segment(SizeR(1,3));
//   A[2] = 2;
// then B[2] == 2 and C[1] == 2 too.

// TODO add capacity_, move ctor
template<typename V>
class SArray {
 public:
  SArray() { }

  // create an array with length n. To initialize the values, call setValue(v) or
  // setZero()
  explicit SArray(size_t n) { resize(n); }

  // zero-copy constructor, namely just copy the pointer
  template <typename W> explicit SArray(const SArray<W>& arr);
  template <typename W> void operator=(const SArray<W>& arr);

  // copy constructors
  void copyFrom(const V* src, size_t size);
  void copyFrom(const SArray<V>& arr) { copyFrom(arr.data(), arr.size()); }

  // might slow but general version
  template <typename ForwardIt>
  void copyFrom(const ForwardIt first, const ForwardIt last);

  // copy from a initializer_list
  template <typename W> SArray(const std::initializer_list<W>& list) {
    copyFrom(list.begin(), list.end());
  }
  template <typename W> void operator=(const std::initializer_list<W>& list) {
    copyFrom(list.begin(), list.end());
  }

  // replace the current data pointer with data. the memory associated with the
  // replaced pointer will be released if no other SArray points to it.
  void reset(V* data, size_t size);

  // if n <= current_size, then only change the size. otherwise, append n -
  // current_size entries (without value initialization)
  void resize(size_t n);

  // slice a [range.begin(), range.end()) segment, zero-copy
  SArray<V> segment(const Range<size_t>& range) const;

  // assume array values are ordered, return *this \cap other. for example:
  //   SArray<int> a{1,2,3,5,6,7,8}, b{3,4,7,10}, c{3,7};
  // then a.setIntersection(b) == c
  SArray<V> setIntersection(const SArray<V>& other) const;

  // assume array values are ordered, return *this \cup other. for example:
  //   SArray<int> a{3,5,8,10}, b{5,9,10,11}, c{3,5,8,9,10,11};
  // then a.setUnion(b) == c
  SArray<V> setUnion(const SArray<V>& other) const;

  // assume array values are ordered. return the position range of the segment
  // whose entry values are within [bound.begin(), bound.end())
  SizeR findRange (const Range<V>& bound) const;

  // accessors and mutators
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
  const shared_ptr<void>& pointer() const { return ptr_; }

  V& operator[] (int i) { return data_[i]; }
  const V& operator[] (int i) const { return data_[i]; }

  template <typename W> bool operator==(const SArray<W> &rhs) const;

  // return an Eigen3 vector, zero-copy
  typedef Eigen::Map<Eigen::Matrix<V, Eigen::Dynamic, 1> > EVecMap;
  EVecMap eigenVector() const { return EVecMap(data(), size()); }

  // return an Eigen3 array, zero-copy
  typedef Eigen::Map<Eigen::Array<V, Eigen::Dynamic, 1> > EArrayMap;
  EArrayMap eigenArray() const { return EArrayMap(data(), size()); }

  // convert to a dense matrix, zero-copy
  shared_ptr<Matrix<V>> matrix(size_t rows = -1, size_t cols = -1);

  // number of non-zero entries
  size_t nnz() const;

  // set all entries into value
  void setValue(V value);
  // set all entries into 0
  void setZero() { memset(data_, 0, size_*sizeof(V)); }

  // return the compressed array by snappy
  SArray<char> compressTo() const;

  // uncompress the values from src with size src_size. Before calling this
  // function, you should allocate enough memory first (e.g. call resize(xx))
  void uncompressFrom(const char* src, size_t src_size);

  // read values from a binary file
  bool readFromFile(const string& file_name, size_t size) {
    return readFromFile(SizeR(0, size), file_name);
  }
  // read the segment [range.begin(), range.end()) from the binary file
  bool readFromFile(SizeR range, const string& file_name);

  // write all values into a binary file
  bool writeToFile(const string& file_name) const {
    return writeToFile(SizeR::all(), file_name);
  }

  // write the segment [range.begin(), range.end()) into a binary file
  bool writeToFile(SizeR range, const string& file_name) const;

 private:
  size_t size_ = 0;
  V* data_ = nullptr;
  shared_ptr<void> ptr_ = shared_ptr<void>(nullptr);
};

// for debug use
template <typename V>
std::ostream& operator<<(std::ostream& os, const SArray<V>& obj) {
  os << dbstr(obj.data(), obj.size(), 10);
  return os;
}



} // namespace PS
