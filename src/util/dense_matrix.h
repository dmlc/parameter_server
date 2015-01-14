#pragma once

#include "util/matrix.h"

namespace PS {

template<typename V>
class DenseMatrix : public Matrix<V> {
 public:
  USING_MATRIX;
  DenseMatrix() { }
  DenseMatrix(size_t rows, size_t cols, bool row_major = true) {
    resize(rows, cols, rows*cols, row_major);
  }

  void resize(size_t rows, size_t cols, size_t nnz, bool row_major);

  DenseMatrix(const MatrixInfo& info, SArray<V> value)
      : Matrix<V>(info, value) { }

  // TODO
  virtual void times(const V* x, V *y) const {CHECK(false);  }

  // C = A .* B
  virtual MatrixPtr<V> dotTimes(const MatrixPtr<V>& B) const { CHECK(false); return MatrixPtr<V>(); }

  // (nearly) non-copy matrix transpose
  virtual MatrixPtr<V> trans() const {CHECK(false);  return MatrixPtr<V>(); }

  // convert global index into local index (0,1,2,3...) and return the key map
  // virtual MatrixPtr<V> localize(SArray<Key>* key_map) const {CHECK(false);  return MatrixPtr<V>(); }

  virtual MatrixPtr<V> alterStorage() const;

  // non-copy matrix block
  virtual MatrixPtr<V> rowBlock(SizeR range) const {
    if (colMajor()) CHECK_EQ(range, SizeR(0, rows()));
    auto info = info_;
    range.to(info.mutable_row());
    info.set_nnz(range.size() * cols());
    return MatrixPtr<V>(new DenseMatrix<V>(info, value_.segment(range*cols())));
  }

  virtual MatrixPtr<V> colBlock(SizeR range) const {
    if (rowMajor()) CHECK_EQ(range, SizeR(0, cols()));
    auto info = info_;
    range.to(info.mutable_col());
    info.set_nnz(range.size() * rows());
    return MatrixPtr<V>(new DenseMatrix<V>(info, value_.segment(range*rows())));
  }

  virtual bool writeToBinFile(string name) const {
    return (writeProtoToASCIIFile(info_, name+".info")
            && value_.writeToFile(name+".value"));
  }

  virtual string debugString() const {
    std::stringstream ss;
    ss << rows() << " x " << cols() << " dense matrix " << std::endl
       << dbstr(value_.data(), value_.size(), 8);
    return ss.str();
  }
};


template<typename V>
void DenseMatrix<V>::resize(
    size_t rows, size_t cols, size_t nnz, bool row_major) {
  info_.set_type(MatrixInfo::DENSE);
  info_.set_row_major(row_major);
  SizeR(0, rows).to(info_.mutable_row());
  SizeR(0, cols).to(info_.mutable_col());
  nnz = rows * cols;
  // CHECK_EQ(nnz, rows*cols);
  info_.set_nnz(nnz);
  info_.set_sizeof_value(sizeof(V));
  // info_.set_nnz_per_row(cols);
  // info_.set_nnz_per_col(rows);
  // data
  value_.resize(nnz);
  value_.setZero();

}


template<typename V>
MatrixPtr<V> DenseMatrix<V>::alterStorage() const {
  size_t in = innerSize();
  size_t out = outerSize();
  CHECK_EQ(value_.size(), in*out);

  SArray<V> new_value(value_.size());

  for (size_t i = 0; i < in; ++i) {
    for (size_t j = 0; j < out; ++j) {
      new_value[i*out+j] = value_[j*in+i];
    }
  }

  auto new_info = info_;
  new_info.set_row_major(!info_.row_major());

  return MatrixPtr<V>(new DenseMatrix<V>(new_info, new_value));
}



} // namespace PS
