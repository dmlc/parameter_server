#pragma once

#include "util/bin_data.h"
#include "util/range.h"
#include "util/common.h"
#include "util/key.h"
#include "util/xarray.h"
#include "util/eigen3.h"
namespace PS {

typedef Range<size_t> Seg;
// row-majored sparse matrix. Usually it is a block of a large sparse matrix.
template<typename I = size_t, typename V = double>
class RSpMat {
 public:
  // read data statistics from file
  static Seg RowSeg(const string& name);
  static Seg ColSeg(const string& name);
  static size_t NNZ(const string& name);
  RSpMat() : offset_(NULL), index_(NULL), value_(NULL) { }
  ~RSpMat() { delete [] offset_; delete [] index_; delete [] value_; }

  // read from binary files
  // name.size : plain text
  // name.rowcnt : size_t format
  // name.colidx : I format
  // name.value : V format
  void Load(const string& name, Seg row = Seg::All());
  // TODO use template
  void ToEigen3(DSMat *mat);
 private:
  DISALLOW_COPY_AND_ASSIGN(RSpMat);
  // the row and column range in the global matrix
  Seg row_;
  Seg col_;
  // number of rows, columns, non-zero entries
  size_t rows_, cols_, nnz_;

  // TODO use unique_ptr or shared_ptr
  // row offset
  size_t* offset_;
  // column index
  I* index_;
  // values
  V* value_;

  // empty in default. will be filled value if
  XArray<Key> global_key_;
};

template<typename I, typename V>
Seg RSpMat<I,V>::RowSeg(const string& name) {
  std::ifstream in(name+".size");
  CHECK(in.good()) << "open " << name << ".size failed.";
  Seg row;
  in >> row.start() >> row.end();
  CHECK(row.Valid()) << "invalid row range " << row.ToString();
  return row;
}

template<typename I, typename V>
Seg RSpMat<I,V>::ColSeg(const string& name) {
  std::ifstream in(name+".size");
  CHECK(in.good()) << "open " << name << ".size failed.";
  Seg row, col;
  in >> row.start() >> row.end() >> col.start() >> col.end();
  CHECK(col.Valid()) << "invalid column range " << col.ToString();;
  return col;
}
template<typename I, typename V>
size_t RSpMat<I,V>::NNZ(const string& name) {
  size_t tmp;
  std::ifstream in(name+".size");
  CHECK(in.good()) << "open " << name << ".size failed.";
  in >> tmp >> tmp >> tmp >> tmp >> tmp;
  return tmp;
}

template<typename I, typename V>
void RSpMat<I,V>::Load(const string& name, Seg row)  {
  // read matrix size
  auto all = RowSeg(name);
  if (row == Seg::All()) {
    row_ = all;
  } else {
    CHECK(row.SubsetEq(all)) << "try to read rows " << row.ToString()
                       << " from data with rows " << all.ToString();
    row_ = row;
  }
  col_ = ColSeg(name);
  rows_ = row_.size();
  cols_ = col_.size();
  nnz_ = NNZ(name);

  // load row offset
  size_t rows = bin_length<size_t>(name+".rowcnt");
  CHECK_GT(rows, 0) << name << ".rowcnt is empty";
  CHECK_EQ(rows, all.size()+1);
  load_bin<size_t>(name+".rowcnt", &offset_, row_.start(), rows_ + 1);

  // load column index
  size_t nnz = load_bin<I>(name+".colidx", &index_, offset_[0], offset_[rows_]);
  CHECK_EQ(nnz, nnz_);

  // load values if any
  size_t nval = bin_length<V>(name+".value");
  if (nval != 0) {
    load_bin<V>(name+".value", &value_, offset_[0], offset_[rows_]);
  }

  // shift the offset if necessary
  size_t start = offset_[0];
  if (start != 0) {
    for (size_t i = 0; i <= rows_; ++i) {
      offset_[i] -= start;
    }
  }

}
template<typename I, typename V>
void RSpMat<I,V>::ToEigen3(DSMat *mat) {
  IVec reserve(rows_);
  for (size_t i = 0; i < rows_; ++i) {
    reserve[i] = (int) (offset_[i+1] - offset_[i]);
  }
  mat->resize(rows_, cols_);
  mat->reserve(reserve);
  for (size_t i = 0; i < rows_; ++i) {
    for (size_t j = offset_[i]; j < offset_[i+1]; ++j) {
      mat->insert(i, index_[j] - col_.start()) =
          value_ == NULL ? 1 : value_[j];
    }
  }
  mat->makeCompressed();
}

} // namespace PS
