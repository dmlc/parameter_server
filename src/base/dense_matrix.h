#pragma once

#include "base/matrix.h"

namespace PS {

template<typename V>
class DenseMatrix : public Matrix<V> {
 public:
  DenseMatrix(size_t rows, size_t cols, bool row_major = true);

  DenseMatrix(const MatrixInfo& info, SArray<V> value)
      : Matrix<V>(info, value) { }

  // TODO
  virtual void times(const double* x, double *y) const { }

  // C = A .* B
  virtual MatrixPtr<V> dotTimes(const MatrixPtr<V>& B) const { return MatrixPtr<V>(); }

  // (nearly) non-copy matrix transpose
  virtual MatrixPtr<V> trans() const { return MatrixPtr<V>(); }

  // convert global index into local index (0,1,2,3...) and return the key map
  virtual MatrixPtr<V> localize(SArray<Key>* key_map) const { return MatrixPtr<V>(); }

  virtual MatrixPtr<V> alterStorage() const { return MatrixPtr<V>(); }

  // non-copy matrix block
  virtual MatrixPtr<V> rowBlock(SizeR range) const { return MatrixPtr<V>(); }
  virtual MatrixPtr<V> colBlock(SizeR range) const { return MatrixPtr<V>(); }

  virtual bool writeToBinFile(string name) const {
    return (WriteProtoToASCIIFile(info_, name+".info") && value_.writeToFile(name+".value"));
  }
 private:
  using Matrix<V>::info_;
  using Matrix<V>::value_;
};

template<typename V>
DenseMatrix<V>::DenseMatrix(size_t rows, size_t cols, bool row_major) {
  // info
  info_.set_type(MatrixInfo::DENSE);
  info_.set_row_major(row_major);
  SizeR(0, rows).to(info_.mutable_row());
  SizeR(0, cols).to(info_.mutable_col());
  info_.set_nnz(rows*cols);
  info_.set_sizeof_value(sizeof(V));
  info_.set_nnz_per_row(cols);
  info_.set_nnz_per_col(rows);
  // data
  value_.resize(rows*cols);
  value_.setZero();
}


} // namespace PS
