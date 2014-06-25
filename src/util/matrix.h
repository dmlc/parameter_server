#pragma once
#include "proto/matrix.pb.h"

namespace PS {
// memory efficient matrix class, but only support limited operations
// the base class
class Matrix {
 public:
  Matrix() { }
  explicit Matrix(const string& info_file) { loadInfo(info_file); }
  explicit Matrix(const MatrixInfo& info) : info_(info) { }
  // Matrix(size_t rows, size_t cols, bool row_major);
  ~Matrix() { }

  bool empty() {
    return (rows() == 0 && cols() == 0);
  }
  inline void loadInfo(const string& info_file);
  const MatrixInfo& info() const { return info_; }
  MatrixInfo& info() { return info_; }

  inline void tranposeInfo();

  size_t rows() const {
    return info_.row_end() - info_.row_begin();
  }
  size_t cols() const {
    return info_.col_end() - info_.col_begin();
  }
  bool bool_value() const {
    return info_.bool_value();
  }

  // size_t localRows() const {
  //   if (info_.localized()) return info_.local_rows(); else return rows();
  // }
  // size_t localCols() const {
  //   if (info_.localized()) return info_.local_cols(); else return cols();
  // }

  bool rowMajor() const {
    return info_.row_major();
  }

  size_t innerBegin() const {
    if (!rowMajor()) return info_.row_begin(); else return info_.col_begin();
  }
  size_t innerEnd() const {
    if (!rowMajor()) return info_.row_end(); else return info_.col_end();
  }

  size_t innerSize() const {
    return (innerEnd() - innerBegin());
  }

  void setInnerBegin(size_t begin) {
    if (!rowMajor()) info_.set_row_begin(begin); else  info_.set_col_begin(begin);
  }
  void setInnerEnd(size_t end) {
    if (!rowMajor()) info_.set_row_end(end); else  info_.set_col_end(end);
  }

  size_t outerBegin() const {
    if (rowMajor()) return info_.row_begin(); else return info_.col_begin();
  }
  size_t outerEnd() const {
    if (rowMajor()) return info_.row_end(); else return info_.col_end();
  }

  size_t outerSize() const {
    return (outerEnd() - outerBegin());
  }

  void setOuterBegin(size_t begin) {
    if (rowMajor()) info_.set_row_begin(begin); else  info_.set_col_begin(begin);
  }

  void setOuterEnd(size_t end) {
    if (rowMajor()) info_.set_row_end(end); else  info_.set_col_end(end);
  }

 protected:
  MatrixInfo info_;
};

void Matrix::loadInfo(const string& info_file) {
  std::ifstream t(info_file);
  CHECK(t.good());
  std::string str((std::istreambuf_iterator<char>(t)),
      std::istreambuf_iterator<char>());
  CHECK(TextFormat::ParseFromString(str, &info_));
}

void Matrix::tranposeInfo() {
  auto info = info_;
  info_.set_row_major(!info.row_major());
  info_.set_row_begin(info.col_begin());
  info_.set_col_begin(info.row_begin());
  info_.set_row_end(info.col_end());
  info_.set_col_end(info.row_end());
}


// std::ostream& operator<<(std::ostream& os, const Matrix& obj) {
//   // os << "size: " << obj.rows() << " x " << obj.cols();
//   // if (obj.row_major())
//   //   os << " in row major";
//   // else
//   //   os << " in col major";
//   // return os;
// }

} // namespace PS
