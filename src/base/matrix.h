#pragma once

#include "Eigen/Dense"
#include "proto/matrix.pb.h"
#include "util/file.h"
#include "base/range.h"
#include "base/shared_array.h"

namespace PS {

// multi-threaded memory efficient matrix class
template<typename V> class Matrix;
template<typename V> using MatrixPtr = std::shared_ptr<Matrix<V>>;
template<typename V> using MatrixPtrList = std::vector<MatrixPtr<V>>;
template<typename V> using MatrixPtrInitList = std::initializer_list<MatrixPtr<V>>;

template<typename V>
class Matrix {
 public:
  explicit Matrix(const MatrixInfo& info) : info_(info) { };
  Matrix(const MatrixInfo& info, const SArray<V>& value)
      : info_(info), value_(value) { };

  // multi-threaded matrix-vector multiplication:
  // y = A * x
  typedef Eigen::Matrix<V, Eigen::Dynamic, 1> EVec;
  EVec operator*(const Eigen::Ref<const EVec>& x) const { return times(x); }

  EVec times(const Eigen::Ref<const EVec>& x) const {
    CHECK_EQ(x.size(), cols());
    EVec y(rows());
    times(x.data(), y.data());
    return y;
  }

  // both x and y are pre-allocated
  virtual void times(const V* x, V *y) const = 0;

  // multi-threaded matrix-vector multiplication:
  // y = A' * x
  EVec transTimes(const Eigen::Ref<const EVec>& x) const {
    return trans()->times(x);
  }

  // elemental-wise times
  // C = A .* B
  virtual MatrixPtr<V> dotTimes(const MatrixPtr<V>& B) const = 0;

  // (nearly) non-copy matrix transpose
  virtual MatrixPtr<V> trans() const = 0;

  // convert global index into local index (0,1,2,3...) and return the key map
  virtual MatrixPtr<V> localize(SArray<Key>* key_map) const = 0;

  // alter between row-major storage and column-major storage
  MatrixPtr<V> toRowMajor() {
    return (rowMajor() ? MatrixPtr<V>(this, [](Matrix<V>* p){}) : alterStorage());
  }
  MatrixPtr<V> toColMajor() {
    return (rowMajor() ? alterStorage() : MatrixPtr<V>(this, [](Matrix<V>* p){}));
  }

  virtual MatrixPtr<V> alterStorage() const = 0;

  // non-copy matrix block
  virtual MatrixPtr<V> rowBlock(SizeR range) const = 0;
  virtual MatrixPtr<V> colBlock(SizeR range) const = 0;

  // I/O see matrix_io.h for more
  virtual bool writeToBinFile(string name) const = 0;

  // accessor and mutators
  MatrixInfo info() const { return info_; }
  uint64 rows() const { return info_.row().end() - info_.row().begin(); }
  uint64 cols() const { return info_.col().end() - info_.col().begin(); }
  uint64 nnz() const { return info_.nnz(); }
  bool rowMajor() const { return info_.row_major(); }
  size_t innerSize() const { return (rowMajor() ? cols() : rows()); }
  size_t outerSize() const { return (rowMajor() ? rows() : cols()); }
  bool empty() const { return (rows() == 0 && cols() == 0); }

  SArray<V> value() const { return value_; }

  void tranposeInfo() {
    auto info = info_;
    info_.set_row_major(!info.row_major());
    *info_.mutable_row() = info.col();
    *info_.mutable_col() = info.row();
  }


 protected:
  MatrixInfo info_;
  SArray<V> value_;
};


}  // namespace PS
