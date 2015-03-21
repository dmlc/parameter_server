#pragma once
#include <atomic>
#include "util/common.h"
#include "util/file.h"
#include "util/range.h"
#include "Eigen/Core"
#include "data/proto/data.pb.h"
#include "parameter/proto/param.pb.h"

namespace PS {

template<typename V> class Matrix;

// static std::atomic<int64> g_mem_usage_sarray = ATOMIC_VAR_INIT(0);
// extern int64 g_mem_usage_sarray;
// extern std::mutex g_mu_sa_;

// Memory efficient array. Most operations are zero-copy, such as assign, slice
// a segment, convert to Eigen3 vector/array. It shares the same semantic as a C
// array pointer. For example,
//   SArray<int> A(10);
//   SArray<int> B = A;
//   SArray<int> C = A.segment(SizeR(1,3));
//   A[2] = 2;
// then B[2] == 2 and C[1] == 2 too.
template<typename V> class SArray {
 public:
  SArray() { }
  ~SArray() { }
  // Create an array with length n. Values are not initialized. To initialize
  // them, call setValue(v) or setZero()
  explicit SArray(size_t n) { resize(n); }
  SArray(size_t n, V val) { resize(n, val); }

  // Zero-copy constructor, namely just copy the pointer
  template <typename W> explicit SArray(const SArray<W>& arr);
  template <typename W> void operator=(const SArray<W>& arr);

  SArray(V* data, size_t size, bool deletable = true) {
    reset(data, size, deletable);
  }

  // Copy constructors
  void CopyFrom(const V* src, size_t size);
  void CopyFrom(const SArray<V>& arr);
  // A general but might slower version
  template <typename ForwardIt>
  void CopyFrom(const ForwardIt first, const ForwardIt last);
  // Copy from a initializer_list
  template <typename W> SArray(const std::initializer_list<W>& list);
  template <typename W> void operator=(const std::initializer_list<W>& list);


  // Slice a [range.begin(), range.end()) segment, zero-copy
  SArray<V> Segment(const Range<size_t>& range) const;
  // Assume all arraies are ordered, return *this \cap other. for example:
  //   SArray<int> a{1,2,3,5,6,7,8}, b{3,4,7,10}, c{3,7};
  // then a.setIntersection(b) == c
  SArray<V> SetIntersection(const SArray<V>& other) const;
  // Assume all arraies are ordered, return *this \cup other. for example:
  //   SArray<int> a{3,5,8,10}, b{5,9,10,11}, c{3,5,8,9,10,11};
  // then a.setUnion(b) == c
  SArray<V> SetUnion(const SArray<V>& other) const;
  // Assume array values are ordered. return the position range of the segment
  // whose entry values are within [bound.begin(), bound.end())
  SizeR FindRange (const Range<V>& bound) const;

  // Capacity
  size_t size() const { return size_; }
  size_t capacity() const { return capacity_; }
  size_t MemSize() const { return capacity_*sizeof(V); }

  // static int64 gMemSize() { return g_mem_usage_sarray.load(); }

  bool empty() const { return size() == 0; }
  // Replace the current data pointer with data. the memory associated with the
  // replaced pointer will be released if no other SArray points to it.
  void reset(V* data, size_t size, bool deletable = true);
  // Resizes the array so that it contains n elements.
  // If n <= capacity_, then only change the size. otherwise, append n -
  // current_size entries (without value initialization)
  void resize(size_t n);
  void resize(size_t n, V val) { resize(n); SetValue(val); }
  // Requests that the capacity be at least enough to contain n elements.
  void reserve(size_t n);
  void clear() { reset(nullptr, 0); }

  // Iterators
  V* begin() { return data(); }
  const V* begin() const { return data(); }
  V* end() { return data() + size(); }
  const V* end() const { return data() + size(); }

  // Element access:
  V back() const { CHECK(!empty()); return data_[size_-1]; }
  V front() const { CHECK(!empty()); return data_[0]; }
  V& operator[] (int i) { return data_[i]; }
  const V& operator[] (int i) const { return data_[i]; }

  // Modifiers
  void append(const SArray<V>& tail);
  void push_back(const V& val);
  void pop_back() { if (size_) --size_; }
  void SetValue(V value);
  // set all entries into 0
  void SetZero() { memset(data_, 0, size_*sizeof(V)); }
  // set values according to *cf*
  void SetValue(const ParamInitConfig& cf);

  // Others
  // Assume values are ordered, return the value range.
  Range<V> range() const {
    return (empty() ? Range<V>(0,0) : Range<V>(front(), back()+1));
  }
  V* data() const { return data_; }
  const std::shared_ptr<void>& pointer() const { return ptr_; }
  // number of non-zero entries
  size_t nnz() const;

  // Compare values
  template <typename W> bool operator==(const SArray<W> &rhs) const;

  // return an Eigen3 vector, zero-copy
  typedef Eigen::Map<Eigen::Matrix<V, Eigen::Dynamic, 1> > EVecMap;
  EVecMap EigenVector() const { return EVecMap(data(), size()); }
  EVecMap vec() const { return EVecMap(data(), size()); }

  // return an Eigen3 array, zero-copy
  typedef Eigen::Map<Eigen::Array<V, Eigen::Dynamic, 1> > EArrayMap;
  EArrayMap EigenArray() const { return EArrayMap(data(), size()); }
  EArrayMap arr() const { return EArrayMap(data(), size()); }

  // return an Eigen3 matrix, zero-copy
  typedef Eigen::Map<Eigen::Array<V, Eigen::Dynamic, Eigen::Dynamic> > EMatMap;
  EMatMap EigenMatrix(int k) const {
    CHECK_EQ(size()%k, 0); return EArrayMap(data(), size()/k, k);
  }
  EMatMap mat(int k) const {
    CHECK_EQ(size()%k, 0); return EArrayMap(data(), size()/k, k);
  }


  double Sum() const { return EigenArray().sum(); }
  double Mean() const { return empty() ? 0 : Sum() / (double)size(); }
  double Std() const {
    return empty() ? 0 :
        (EigenArray() - Mean()).matrix().norm() / sqrt((double)size());
  }

  // convert to a dense matrix, zero-copy
  std::shared_ptr<Matrix<V>> SMatrix(size_t rows = -1, size_t cols = -1);

  // Return the compressed array by snappy
  SArray<char> CompressTo() const;
  // Uncompress the values from src with size src_size. Before calling this
  // function, you should allocate enough memory first (e.g. call resize(xx))
  void UncompressFrom(const char* src, size_t src_size);
  void UncompressFrom(const SArray<char>& src) { UncompressFrom(src.data(), src.size()); }

  // read the segment [range.begin(), range.end()) from the binary file
  bool ReadFromFile(SizeR range, const string& file_name);
  bool ReadFromFile(const string& file_name) {
    return ReadFromFile(SizeR::all(), file_name);
  }
  bool ReadFromFile(SizeR range, const DataConfig& file);

  // write all values into a binary file
  bool WriteToFile(const string& file_name) const {
    return WriteToFile(SizeR(0, size_), file_name);
  }
  // write the segment [range.begin(), range.end()) into a binary file
  bool WriteToFile(SizeR range, const string& file_name) const;


 private:
  size_t size_ = 0;
  size_t capacity_ = 0;
  V* data_ = nullptr;
  std::shared_ptr<void> ptr_ = std::shared_ptr<void>(nullptr);


};

// for debug use
template <typename V>
std::ostream& operator<<(std::ostream& os, const SArray<V>& obj) {
  os << dbstr(obj.data(), obj.size(), 10);
  return os;
}

} // namespace PS
