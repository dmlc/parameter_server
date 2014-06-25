#pragma once

#include <thread>

#include "Eigen/Dense"
#include "util/common.h"
#include "util/matrix.h"
#include "util/bin_data.h"
#include "base/shared_array.h"
#include "base/range.h"

// #include "eigen3/Eigen/src/Core/Ref.h"
// #include "proto/matrix.pb.h"

// #include "util/eigen3.h"
namespace PS {


// sparse matrix with Yale format
template<typename I = int, typename V = double>
class SparseMatrix : public Matrix {
 public:
  typedef Eigen::Matrix<V, Eigen::Dynamic, 1> EVec;
  SparseMatrix() { }
  SparseMatrix(const MatrixInfo& info) : Matrix(info) { }
  SparseMatrix(const MatrixInfo& info,
               const SArray<size_t>& offset,
               const SArray<I>& index,
               const SArray<V>& value)
      : Matrix(info), offset_(offset), index_(index), value_(value) { }

  ~SparseMatrix() { }

  void load(const string& name) {
    loadInfo(name + ".info");
    load(name, SizeR(outerBegin(), outerEnd()));
  }
  void load(const string& name, const SizeR& range);

  // y = A * x
  EVec operator*(const Eigen::Ref<const EVec>& x);

  void rowMatrixMultiplyVct(const Eigen::Ref<const EVec>& x, EVec *y);
  void colMatrixMultiplyVct(const Eigen::Ref<const EVec>& x, EVec *y);
  void rowMatrixMultiplyVctThreadFunc(const Eigen::Ref<const EVec>& x,
                                      EVec* y,
                                      size_t start, size_t end);
  void colMatrixMultiplyVctThreadFunc(const Eigen::Ref<const EVec>& x,
                                      EVec* y, int mod);

  // matrix transpose , near zero-overhead
  SparseMatrix<I,V> trans() {
    auto B = *this;
    B.tranposeInfo();
    return B;
  }

  // convert global index into local index (0,1,2,3...) and return the maping key
  SparseMatrix<uint32,V> localize(SArray<I>* key);

  // change between row-major storage and column-major storage
  SparseMatrix<I,V> alterStorage();

  SparseMatrix<I,V> colBlock(SizeR range);
  SparseMatrix<I,V> rowBlock(SizeR range);

  // coeefficient wise mutation
  template<class UniFn> SparseMatrix<I,V> cwise(UniFn fn) {
    SArray<V> new_value;
    new_value.copyFrom(value_);
    // for (auto it = new_value.begin(); it != new_value.end(); ++it) { fn(*it); }
    for (auto& v : new_value) { fn(v); }
    return SparseMatrix<I,V>(info_, offset_, index_, new_value);
  }

  V* value() { return value_.data(); }

  // debug string
  string debugString() const {
    std::stringstream ss;
    int nnz = offset_.back() - offset_.front();
    ss << info_.DebugString() << "offset: " << offset_ << "\n"
       << "index: " << dbstr(index_.data()+offset_[0], nnz)  << "\n"
       << "value: " << dbstr(value_.data()+offset_[0], nnz)  << "\n";
    return ss.str();
  }
 private:
  SArray<size_t> offset_;
  SArray<I> index_;
  SArray<V> value_;
};

// template<typename I, typename V>
// void rowMatrixMultiplyVctThreadFunc(SparseMatrix<I,V> *matrix,
//                                     const Eigen::Ref<const Eigen::Matrix<V, Eigen::Dynamic, 1> >& x,
//                                     Eigen::Ref<const Eigen::Matrix<V, Eigen::Dynamic, 1> > *y,
//                                     size_t start, size_t end) {
//   bool bool_value = matrix->bool_value();
//   V *value = matrix->value();
//   I *index = matrix->index();
//   size_t *offset = matrix->offset();
//   for (size_t i = start; i < end; ++i) {
//     if (offset[i] == offset[i+1]) continue;
//     V v = 0;
//     // put the if outside to reduce the branch mis-prediction
//     if (bool_value) {
//       for (size_t j = offset[i]; j < offset[i+1]; ++j)
//         v += x.coeff(index[j]);
//     } else {
//       for (size_t j = offset[i]; j < offset[i+1]; ++j) {
//         v += x.coeff(index[j]) * value[j];
//       }
//     }
//     y->coeffRef(i) = v;
//   }
// }
//
// template<typename I, typename V>
// void rowMatrixMultiplyVct(SparseMatrix<I,V> *matrix,
//                           const Eigen::Ref<const Eigen::Matrix<V, Eigen::Dynamic, 1> >& x,
//                           Eigen::Ref<const Eigen::Matrix<V, Eigen::Dynamic, 1> > *y) {
//   int num_threads = 8;
//   size_t offset = matrix->offset().size() - 1;
//   if (offset < 256) {
//     num_threads = 1;
//   }
//   std::thread *tid[num_threads];
//   size_t average_workload = offset / num_threads;
//   for (int t = 0; t < num_threads; ++t) {
//     size_t start = average_workload * t;
//     size_t end = start + average_workload;
//     if (t == num_threads - 1) {
//       end = offset;
//     }
//     tid = new std::thread(&rowMatrixMultiplyVctThreadFunc<I, V>, x, y, start, end);
//   }
//   for (int t = 0; t < num_threads; ++t) {
//     tid[t].join();
//   }
//   return 0;
// }

template<typename I, typename V>
void SparseMatrix<I,V>::colMatrixMultiplyVctThreadFunc(const Eigen::Ref<const EVec>& x,
                                    EVec *y, int mod) {
  bool bool_value = info_.bool_value();
  V *value = value_.data();
  I *index = index_.data();
  size_t *offset = offset_.data();

  for (size_t i = 0; i < offset_.size()-1; ++i) {
    if (offset[i] == offset[i+1]) continue;
    V v = x.coeff(i);
    if (bool_value) {
      for (size_t j = offset[i]; j < offset[i+1]; ++j) {
        if (index[j] % mod != 0) { continue; }
        y->coeffRef(index[j]) += v;
      }
    } else {
      for (size_t j = offset[i]; j < offset[i+1]; ++j) {
        if (index[j] % mod != 0) { continue; }
        y->coeffRef(index[j]) += v * value[j];
      }
    }
  }
}

template<typename I, typename V>
void SparseMatrix<I,V>::colMatrixMultiplyVct(const Eigen::Ref<const EVec>& x,
                                             EVec* y) {
  int num_threads = FLAGS_num_threads;
  std::thread *tid[num_threads];
  for (int t = 1; t < num_threads+1; ++t) {
    tid[t] = new std::thread([this, x, y, t](){colMatrixMultiplyVctThreadFunc(x, y, t);});
  }
  for (int t = 0; t < num_threads; ++t) {
    tid[t]->join();
  }
  // return 0;
}

template<typename I, typename V>
void SparseMatrix<I,V>::rowMatrixMultiplyVctThreadFunc(const Eigen::Ref<const EVec>& x,
                                                  EVec* y,
                                                  size_t start, size_t end) {
  bool bool_value = info_.bool_value();
  V *value = value_.data();
  I *index = index_.data();
  size_t *offset = offset_.data();
  for (size_t i = start; i < end; ++i) {
    if (offset[i] == offset[i+1]) continue;
    V v = 0;
    // put the if outside to reduce the branch mis-prediction
    if (bool_value) {
      for (size_t j = offset[i]; j < offset[i+1]; ++j)
        v += x.coeff(index[j]);
    } else {
      for (size_t j = offset[i]; j < offset[i+1]; ++j) {
        v += x.coeff(index[j]) * value[j];
      }
    }
    y->coeffRef(i) = v;
  }
}

template<typename I, typename V>
void SparseMatrix<I,V>::rowMatrixMultiplyVct(const Eigen::Ref<const EVec>& x,
                                             EVec* y) {
  int num_threads = FLAGS_num_threads;
  size_t offset = offset_.size() - 1;
  if (offset < 256) {
    num_threads = 1;
  }
  // fprintf(stderr, "num thread %d\n", num_threads);
  std::thread *tid[num_threads];
  size_t average_workload = offset / num_threads;
  for (int t = 0; t < num_threads; ++t) {
    size_t start = average_workload * t;
    size_t end = start + average_workload;
    if (t == num_threads - 1) {
      end = offset;
    }
    tid[t] = new std::thread([this, x, y, start, end](){rowMatrixMultiplyVctThreadFunc(x, y, start, end);});
  }
  for (int t = 0; t < num_threads; ++t) {
    tid[t]->join();
  }
}

template<typename I, typename V>
typename SparseMatrix<I,V>::EVec
SparseMatrix<I,V>::operator*(const Eigen::Ref<const EVec>& x) {
  CHECK_EQ(x.size(), cols());
  EVec y(rows());
  y.setZero(rows());

  bool bool_value = info_.bool_value();
  if (info_.row_major()) {
    if (FLAGS_num_threads > 1) {
      rowMatrixMultiplyVct(x, &y);
    } else {
      for (size_t i = 0; i < offset_.size()-1; ++i) {
        if (offset_[i] == offset_[i+1]) continue;
        V v = 0;
        // put the if outside to reduce the branch mis-prediction
        if (bool_value) {
          for (size_t j = offset_[i]; j < offset_[i+1]; ++j)
            v += x.coeff(index_[j]);
        } else {
          for (size_t j = offset_[i]; j < offset_[i+1]; ++j) {
            v += x.coeff(index_[j]) * value_[j];
          }
        }
        y.coeffRef(i) = v;
      }
    }
  } else {
    for (size_t i = 0; i < offset_.size()-1; ++i) {
      if (offset_[i] == offset_[i+1]) continue;
      V v = x.coeff(i);
      if (bool_value) {
        for (size_t j = offset_[i]; j < offset_[i+1]; ++j) {
          y.coeffRef(index_[j]) += v;
        }
      } else {
        for (size_t j = offset_[i]; j < offset_[i+1]; ++j) {
          y.coeffRef(index_[j]) += v * value_[j];
        }
      }
    }
  }
  return y;
}

template<typename I, typename V>
void SparseMatrix<I,V>::load(const string& name, const SizeR& range)  {
  loadInfo(name + ".info");
  // testing
  CHECK(range.valid());
  CHECK(info_.sparse());
  CHECK_EQ(info_.sizeof_index() , sizeof(I));
  CHECK_EQ(info_.sizeof_value() , sizeof(V));
  CHECK_LE(outerBegin()         , range.begin());
  CHECK_GE(outerEnd()           , range.end());
  CHECK_LE(range.size()         , bin_length<size_t>(name+".offset"));

  // offset
  size_t* os = nullptr;
  size_t n = range.size() + 1;
  load_bin<size_t>(name + ".offset", &os, range.begin(), n);
  offset_.reset(os, n);
  size_t os0 = offset_[0];

  // index
  size_t nnz = offset_[n-1] - os0;
  I* ix = nullptr;
  load_bin<I>(name + ".index", &ix, os0, nnz);
  index_.reset(ix, nnz);

  // value
  if (!info_.bool_value()) {
    V* vl = nullptr;
    load_bin<V>(name + ".value", &vl, os0, nnz);
    value_.reset(vl, nnz);
  }

  // shift the offset if necessary
  if (os0 != 0) {
    for (size_t i = 0; i < n; ++i)
      offset_[i] -= os0;
  }

  // reset outer size
  info_.set_entries(nnz);
  setOuterBegin(range.begin());
  setOuterEnd(range.end());
}

template<typename I, typename V>
SparseMatrix<uint32,V> SparseMatrix<I,V>::localize(SArray<I>* key) {
  if (info_.localized()) {
    return SparseMatrix<uint32,V>(
        info_, offset_, SArray<uint32>(index_), value_);
  }

  // // build the key map
  // std::map<I, uint32> key_map;
  // for (I k : index_) key_map[k] = kuint32max;

  // size_t n = key_map.size();
  // CHECK_LT(n, (size_t)kuint32max);
  // uint32 i = 0;
  // for (auto& it : key_map) {
  //   (*key)[i] = it.first;
  //   it.second = ++i;
  // }
  // CHECK_EQ(n, (size_t)i);

  CHECK_LT(innerEnd(), (size_t)kuint32max) << "use a hash_map implementation";

  std::vector<uint32> key_map(innerEnd());
  for (I k : index_) key_map[k] = kuint32max;
  uint32 n =  0;
  for (auto& k : key_map) if (k == kuint32max) ++n;

  key->resize(n);
  n = 0;
  for (I i = 0; i < key_map.size(); ++i) {
    if (key_map[i] == kuint32max) {
      (*key)[n] = i;
      key_map[i] = n ++;
    }
  }

  // remap index
  SArray<uint32> local_index(index_.size());
  for(size_t j = 0; j < index_.size(); ++j)
    local_index[j] = key_map[index_[j]];


  // set info
  SparseMatrix<uint32, V> local(info_, offset_, local_index, value_);
  local.info().set_localized(true);
  local.setInnerBegin(0);
  local.setInnerEnd(n);
  return local;
}

template<typename I, typename V>
SparseMatrix<I,V> SparseMatrix<I,V>::alterStorage() {
  CHECK(info_.localized() || innerSize() < (size_t)kuint32max);

  auto new_info = info_;
  new_info.set_row_major(!info_.row_major());

  if (empty()) return SparseMatrix<I,V>(new_info);

  bool bool_value = info_.bool_value();
  size_t n_inner = innerSize();
  size_t n_outer = outerSize();
  SArray<I> new_index(index_.size());
  SArray<V> new_value(value_.size());
  SArray<size_t> new_offset(n_inner+1);
  std::memset(new_offset.data(), 0, sizeof(size_t)*(n_inner+1));

  for (I k : index_) {
    ++ new_offset[k+1];
  }
  for (size_t i = 0; i < n_inner; ++i) {
    new_offset[i+1] += new_offset[i];
  }
  for (size_t i = 0; i < n_outer; ++i) {
    for (size_t j = offset_[i]; j < offset_[i+1]; ++j) {
      I k = index_[j];
      if (!bool_value) new_value[new_offset[k]] = value_[j];
      new_index[new_offset[k]++] = (I) i;
    }
  }
  for (size_t i = n_inner -1; i > 0; --i) {
    new_offset[i] = new_offset[i-1];
  }
  new_offset[0] = 0;

  return SparseMatrix<I,V>(new_info, new_offset, new_index, new_value);
}

template<typename I, typename V>
SparseMatrix<I,V> SparseMatrix<I,V>::colBlock(SizeR range) {
  CHECK(!rowMajor());
  CHECK(!range.empty());

  auto new_offset = offset_.segment(range.begin(), range.size()+1);

  auto new_info = info_;
  new_info.set_col_begin(range.begin());
  new_info.set_col_end(range.end());
  new_info.set_entries(new_offset.back() - new_offset.front());

  return SparseMatrix<I,V>(new_info, new_offset, index_, value_);
}

template<typename I, typename V>
SparseMatrix<I,V> SparseMatrix<I,V>::rowBlock(SizeR range) {
  CHECK(rowMajor());
  CHECK(!range.empty());

  auto new_offset = offset_.segment(range.begin(), range.size()+1);

  auto new_info = info_;
  new_info.set_row_begin(range.begin());
  new_info.set_row_end(range.end());
  new_info.set_entries(new_offset.back() - new_offset.front());

  return SparseMatrix<I,V>(new_info, new_offset, index_, value_);
}

template<typename I, typename V>
std::ostream& operator<<(std::ostream& os, const SparseMatrix<I,V>& mat) {
  os << mat.debugString();
  return os;
}

// template<class UniFn>
// template<typename I, typename V>
// SparseMatrix<I,V> SparseMatrix<I,V>::cwise(UniFn fn) {
//   SparseMatrix<I,V> res = *this;
//   for (auto& v : res.value_) { fn(v); }
//   return res;
// }

// template<typename I, typename V>
// template<typename Derived, typename OtherDerived>
// void SparseMatrix<I,V>::TransposeTimes(const Eigen::DenseBase<Derived>& a,
//                                        Eigen::DenseBase<OtherDerived> const& b) {
//   FastTranspose();
//   Times(a, b);
//   FastTranspose();
// }


  // // return if it is an empty matrix
  // bool Empty() const { return (offset_ == NULL && index_ == NULL && value_ == NULL); }
  // // perform b = A * a
  // DVec Times(DVec a);

  // // read from binary files
  // // name.size : plain text
  // // name.rowcnt : size_t format
  // // name.colidx : I format
  // // name.value : V format
  // void Load(const string& name, Seg part = Seg::All(), bool row_major = true);
  // void Save(const string& name);

  // void RowBlock(const Seg& part, SparseMatrix<I,V> *out);

  // // randomly permute the rows if in row major, otherwise, permute the columns
  // void RandomPermute();

  // // select columns (dim=1) or rows (dim=0) specified by index
  // template<typename Derived>
  // void Select(int dim, const Eigen::DenseBase<Derived>& index, SparseMatrix<I,V> *out);

  // bool has_value() const { return value_ != NULL; }

  // // do transposition, keey the storage ordering
  // void Transpose(SparseMatrix<I,V> *out);
  // // do fast transposition with little cost
  // // it swaps the row size with the column size, but also change the storage ordering
  // void FastTranspose();

  // // some basic linear algebra:
  // // p-norm by viewing as a vector. inf norm if p < 0
  // double Norm(double p);
  // // row-wise (dim=0) or column-wise (dim=1) p-norm
  // Eigen::VectorXd Norm(int dim, double p);

  // // b = X * a
  // // http://eigen.tuxfamily.org/dox/TopicFunctionTakingEigenTypes.html
  // template<typename Derived, typename OtherDerived>
  // void Times(const Eigen::DenseBase<Derived>& a, Eigen::DenseBase<OtherDerived> const& b);
  // // b = X' * a
  // template<typename Derived, typename OtherDerived>
  // void TransposeTimes(const Eigen::DenseBase<Derived>& a,
  //                     Eigen::DenseBase<OtherDerived> const& b);

  // size_t* offset() { return offset_; }
  // I* index() { return index_; }
  // V* value() { return value_; }
  // // return number of non-zero entries
  // size_t nnz() const { return (offset_[outer_size()] - offset_[0]); }

  // // convert to eigen3 format.., ugly codes... tidy it TODO
  // void ToEigen3(DSMat *mat);

  // void ToEigen3(const std::vector<size_t>& row_index, DSMat *mat);
  // void ToEigen3(const std::vector<size_t>& row_index, CDSMat *mat);

  // // convert to a dense matrix
  // template<typename Derived>
  // void ToEigen3(const std::vector<size_t>& row_index,
  //               Eigen::MatrixBase<Derived> *mat);
// template<typename I, typename V>
// void SparseMatrix<I,V>::Load(const string& name, Seg part, bool row_major)  {
//   row_major_ = row_major;
//   is_segment_ = false;
//   string cnt_name, idx_name;
//   if (row_major_) {
//     cnt_name = name + ".rowcnt";
//     idx_name = name + ".colidx";
//   } else {
//     cnt_name = name + ".colcnt";
//     idx_name = name + ".rowidx";
//   }
//   LoadSize(name);
//   // do some test...
//   size_t nval = bin_length<V>(name+".value");
//   size_t ncnt = bin_length<size_t>(cnt_name);
//   size_t nidx = bin_length<I>(idx_name);
//   size_t nnz = Matrix::Entries(name);
//   if (nval != 0) CHECK_EQ(nval, nnz);
//   CHECK_EQ(nidx, nnz);
//   CHECK_EQ(ncnt, outer_size()+1);

//   if (part != Seg::All()) {
//     if (row_major) {
//       CHECK(part.SubsetEq(row_))
//           << "cannot read rows " << part << " from data with rows " << row_;
//       Resize(part, col_);
//     } else {
//       CHECK(part.SubsetEq(col_))
//           << "cannot read columns " << part << " from data with columns " << col_;
//       Resize(row_, part);
//     }
//   }
//   // load row offset
//   load_bin<size_t>(cnt_name, &offset_, row_.start(), rows_ + 1);
//   // load column index
//   nnz = offset_[rows_] - offset_[0];
//   load_bin<I>(idx_name, &index_, offset_[0], nnz);
//   // load values if any
//   if (nval != 0) {
//     load_bin<V>(name+".value", &value_, offset_[0], nnz);
//   }
//   // shift the offset if necessary
//   size_t start = offset_[0];
//   if (start != 0) {
//     for (size_t i = 0; i <= rows_; ++i) {
//       offset_[i] -= start;
//     }
//   }
// }

// template<typename I, typename V>
// void SparseMatrix<I,V>::Save(const string& name) {
//   SaveSize(name, nnz());
//   string cnt_name, idx_name;
//   if (row_major_) {
//     cnt_name = name + ".rowcnt";
//     idx_name = name + ".colidx";
//   } else {
//     cnt_name = name + ".colcnt";
//     idx_name = name + ".rowidx";
//   }
//   save_bin<size_t>(cnt_name, offset_, rows_+1);
//   save_bin<I>(idx_name, index_, nnz());
//   if (has_value()) save_bin<V>(name+".value", value_, nnz());
// }

// template<typename I, typename V>
// void SparseMatrix<I,V>::clear() {
//   if (!deletable_) {
//     delete [] offset_;
//     delete [] index_;
//     delete [] value_;
//   }
//   offset_ = nullptr;
//   index_ = nullptr;
//   value_ = nullptr;
// }


// template<typename I, typename V>
// void SparseMatrix<I,V>::RandomPermute() {
//   size_t n = outer_size();
//   // generate a random permutation
//   std::vector<size_t> perm(n);
//   for (size_t i = 0; i < n; ++i) perm[i] = i;
//   random_shuffle(perm.begin(), perm.end());
//   // do the copying
//   size_t* new_offset = new size_t[n];
//   new_offset[0] = 0;
//   I* new_index = new I[nnz()+5];
//   V* new_value = NULL;
//   if (has_value()) new_value = new V[nnz()+5];
//   for (size_t i = 0; i < n; ++i) {
//     size_t k = perm[i];
//     size_t start = offset_[k];
//     size_t len = offset_[k+1] - start;
//     size_t new_start = new_offset[i];
//     new_offset[i+1] = new_start + len;
//     memcpy(new_index + new_start, index_ + start, len * sizeof(I));
//     if (has_value())
//       memcpy(new_value + new_start, value_ + start, len * sizeof(V));
//   }
//   Clear();
//   offset_ = new_offset;
//   index_ = new_index;
//   value_ = new_value;
// }

// template<typename I, typename V>
// template<typename Derived>
// void SparseMatrix<I,V>::Select(int dim, const Eigen::DenseBase<Derived>& index,
//                                SparseMatrix<I,V> *out) {
//   CHECK(dim == 0 || dim == 1);
//   CHECK_EQ(index.cols(), 1);
//   if ((row_major_ && dim==0) || (!row_major_ && dim==2)) {
//     CHECK(false) << "the easy one";
//   } else {
//     size_t p = inner_size();
//     CHECK_EQ(index.rows(), p);
//     I *map = new I[p];
//     size_t ap = 0;
//     for (size_t i = 0; i < p; ++i)
//       map[i] = index[i] != 0 ? ap++ : -1;
//     size_t *new_offset = new size_t[outer_size()+1];
//     new_offset[0] = 0;
//     I *new_index = new I[nnz()+5];
//     V *new_value = value_ == NULL ? NULL : new V[nnz()+5];
//     size_t pos = 0;
//     for (size_t i = 0; i < outer_size(); ++i) {
//       for (size_t j = offset_[i]; j < offset_[i+1]; ++j) {
//         I k = index_[j];
//         if (map[k] == -1) continue;
//         new_index[pos] = map[k];
//         if (value_ != NULL) new_value[pos] = value_[j];
//         ++ pos;
//       }
//       new_offset[i+1] = pos;
//     }
//     if (dim == 0)
//       out->Resize(ap, cols_);
//     else
//       out->Resize(rows_, ap);
//     out->row_major_ = row_major_;
//     out->offset_ = new_offset;
//     out->index_ = new_index;
//     out->value_ = new_value;
//     out->is_segment_ = false;
//   }
// }

// template<typename I, typename V>
// void SparseMatrix<I,V>::RowBlock(const Seg& part, SparseMatrix<I,V> *out) {
//   CHECK(row_major_);
//   CHECK(part.Valid());
//   Seg row(0, rows_);
//   CHECK(part.SubsetEq(row)) << part.ToString() << " is not subseteq " << row.ToString();
//   out->row_        = part;
//   out->col_        = col_;
//   out->offset_     = offset_ + part.start();
//   out->index_      = index_;
//   out->value_      = value_;
//   out->rows_       = part.size();
//   out->cols_       = col_.size();
//   out->is_segment_ = true;
//   out->row_major_  = true;
// }

// template<typename I, typename V>
// void SparseMatrix<I,V>::FastTranspose() {
//   std::swap(row_, col_);
//   std::swap(rows_, cols_);
//   row_major_ = !row_major_;
// }

// template<typename I, typename V>
// void SparseMatrix<I,V>::Transpose(SparseMatrix<I,V> *o) {
//   o->Clear();
//   o->CopyFrom(*this);
//   o->FastTranspose();
//   o->row_major_ = !o->row_major_;
//   size_t nout = outer_size();
//   size_t nin = inner_size();
//   o->offset_ = new size_t[nin+5];
//   memset(o->offset_, 0, sizeof(size_t)*(nin+1));
//   for (size_t i = offset_[0]; i < offset_[nout]; ++i)
//     o->offset_[index_[i]+1] ++;
//   for (size_t i = 0; i < nin; ++i)
//     o->offset_[i+1] += o->offset_[i];
//   o->index_ = new I[nnz()+5];
//   if (has_value()) o->value_ = new V[nnz()+5];
//   for (size_t i = 0; i < nout; ++i) {
//     for (size_t j = offset_[i]; j < offset_[i+1]; ++j) {
//       I k = index_[j];
//       o->index_[o->offset_[k]] = (I) i;
//       if (has_value())
//         o->value_[o->offset_[k]] = value_[j];
//       o->offset_[k] ++;
//     }
//   }
//   for (size_t i = nin - 1; i > 0; --i)
//     o->offset_[i] = o->offset_[i-1];
//   o->offset_[0] = 0;
//   o->is_segment_ = false;
//   CHECK_EQ(o->nnz(), nnz());
// }


// template<typename I, typename V>
// double SparseMatrix<I,V>::Norm(double p) {
//   double norm = 0;
//   for (size_t i = offset_[0]; i < offset_[outer_size()]; ++i) {
//     double v = value_ == NULL ? 1.0 : std::abs((double) value_[i]);
//     norm = p >= 0 ? norm + std::pow(v, p) : std::max(norm, v);
//   }
//   return (p > 0 ? std::pow(norm,1.0/p) : norm);
// }

// template<typename I, typename V>
// Eigen::VectorXd SparseMatrix<I,V>::Norm(int dim, double p) {
//   Eigen::VectorXd norm;
//   if ((dim == 0 && row_major_) || (dim == 1 && !row_major_)) {
//     size_t n = outer_size();
//     norm.setZero(n);
//     SparseMatrix<I,V> row;
//     for (size_t i = 0; i < n; ++i) {
//       RowBlock(Seg(i, i+1), &row);
//       norm[i] = row.Norm(p);
//     }
//   } else {
//     norm.setZero(inner_size());
//     for (size_t i = 0; i < outer_size(); ++i) {
//       for (size_t j = offset_[i]; j < offset_[i+1]; ++j) {
//         I k = index_[j];
//         double v = value_ == NULL ? 1.0 : std::abs((double) value_[j]);
//         norm[k] = p >= 0 ? norm[k] + std::pow(v, p) : std::max(norm[k], v);
//       }
//     }
//     if (p > 0) {
//       for (size_t i = 0; i < inner_size(); ++i)
//         norm[i] = std::pow(norm[i], 1.0/p);
//     }
//   }
//   return norm;
// }

// template<typename I, typename V>
// DVec SparseMatrix<I,V>::Times(DVec a) {
//   CHECK(row_major_) << "..";
//   CHECK_EQ(a.size(), cols_);
//   DVec b = DVec::Zero(rows_);
//   // TODO ignored col_.start()...
//   for (size_t i = 0; i < rows_; ++i) {
//     for (size_t j = offset_[i]; j < offset_[i+1]; ++j) {
//       b[i] += a[index_[j]] * (value_ == NULL ? 1 : value_[j]);
//     }
//   }
//   return b;
// }

// template<typename I, typename V>
// template<typename Derived, typename OtherDerived>
// void SparseMatrix<I,V>::Times(const Eigen::DenseBase<Derived>& a,
//                               Eigen::DenseBase<OtherDerived> const& b) {
//   CHECK_EQ(a.rows(), cols_);
//   CHECK_EQ(a.cols(), 1);
//   // a writable b
//   Eigen::DenseBase<OtherDerived>& wb = const_cast<Eigen::DenseBase<OtherDerived>&> (b);
//   // wb.derived().setZero(rows_);
//   wb.derived().resize(rows_);
//   wb.setZero();
//   typedef typename OtherDerived::Scalar V2;
//   if (row_major_) {
//     for (size_t i = 0; i < rows_; ++i) {
//       if (offset_[i] == offset_[i+1]) continue;
//       V2 v = 0;
//       // put the if outside to reduce the misbranch prediction
//       if (has_value()) {
//         for (size_t j = offset_[i]; j < offset_[i+1]; ++j) {
//           v += a.coeff(index_[j]) * value_[j];
//         }
//       } else {
//         for (size_t j = offset_[i]; j < offset_[i+1]; ++j) {
//           v += a.coeff(index_[j]);
//         }
//       }
//       wb.coeffRef(i) = v;
//     }
//   } else {
//     for (size_t i = 0; i < cols_; ++i) {
//       if (offset_[i] == offset_[i+1]) continue;
//       V2 v = a.coeff(i);
//       if (has_value()) {
//         for (size_t j = offset_[i]; j < offset_[i+1]; ++j) {
//           wb.coeffRef(index_[j]) += v * value_[j];
//         }
//       } else {
//         for (size_t j = offset_[i]; j < offset_[i+1]; ++j) {
//           wb.coeffRef(index_[j]) += v;
//         }
//       }
//     }
//   }
// }


// template<typename I, typename V>
// template<typename Derived, typename OtherDerived>
// void SparseMatrix<I,V>::TransposeTimes(const Eigen::DenseBase<Derived>& a,
//                                        Eigen::DenseBase<OtherDerived> const& b) {
//   FastTranspose();
//   Times(a, b);
//   FastTranspose();
// }

// template<typename I, typename V>
// std::ostream& operator<<(std::ostream& os, const SparseMatrix<I,V>& obj) {
//   const Matrix& mat = static_cast<const Matrix&>(obj);
//   os << "sparse matrix with " << mat << ", nnz " << obj.nnz() << ", index size " <<
//       sizeof(I);
//   if (obj.has_value())
//     os << ", value size " << sizeof(V);
//   else
//     os << ", binary value.";
//   return os;
// }
// template<typename I, typename V>
// void SparseMatrix<I,V>::ToEigen3(DSMat *mat) {
//   IVec reserve(rows_);
//   for (size_t i = 0; i < rows_; ++i) {
//     reserve[i] = (int) (offset_[i+1] - offset_[i]);
//   }
//   mat->resize(rows_, cols_);
//   mat->reserve(reserve);
//   for (size_t i = 0; i < rows_; ++i) {
//     for (size_t j = offset_[i]; j < offset_[i+1]; ++j) {
//       mat->insert(i, index_[j] - col_.start()) =
//           value_ == NULL ? 1 : value_[j];
//     }
//   }
//   mat->makeCompressed();
// }

// template<typename I, typename V>
// void SparseMatrix<I,V>::ToEigen3(const std::vector<size_t>& row_index, CDSMat *mat) {
//   size_t rows  = row_index.size();
//   IVec reserve = IVec::Zero(cols_);
//   for (size_t i = 0; i < rows; ++i) {
//     for (size_t j = offset_[row_index[i]]; j < offset_[row_index[i]+1]; ++j) {
//       reserve[index_[j]] ++;
//     }
//   }
//   mat->resize(rows, cols_);
//   mat->reserve(reserve);
//   for (size_t i = 0; i < rows; ++i) {
//     for (size_t j = offset_[row_index[i]]; j < offset_[row_index[i]+1]; ++j) {
//       mat->insert(i, index_[j] - col_.start()) =
//           value_ == NULL ? 1 : value_[j];
//     }
//   }
//   mat->makeCompressed();
// }

// // template<typename I, typename V>
// template<typename Derived>
// void SparseMatrix<I,V>::ToEigen3(const std::vector<size_t>& row_index,
//                            Eigen::MatrixBase<Derived> *mat) {
//   size_t rows = row_index.size();
//   auto& m = mat->derived();
//   //  mat->::Matrix::resize(rows, cols_);
//   m.setZero(rows, cols_);
//   for (size_t i = 0; i < rows; ++i) {
//     for (size_t j = offset_[row_index[i]]; j < offset_[row_index[i]+1]; ++j) {
//       m.coeffRef(i, index_[j] - col_.start()) =
//           value_ == NULL ? 1 : value_[j];
//     }
//   }
// }

// template<typename I, typename V>
// void SparseMatrix<I,V>::ToEigen3(const std::vector<size_t>& row_index, DSMat *mat) {
//   size_t rows = row_index.size();
//   IVec reserve(rows);
//   for (size_t i = 0; i < rows; ++i) {
//     reserve[i] = (int) (offset_[row_index[i]+1] - offset_[row_index[i]]);
//   }
//   mat->resize(rows, cols_);
//   mat->reserve(reserve);
//   for (size_t i = 0; i < rows; ++i) {
//     for (size_t j = offset_[row_index[i]]; j < offset_[row_index[i]+1]; ++j) {
//       mat->insert(i, index_[j] - col_.start()) =
//           value_ == NULL ? 1 : value_[j];
//     }
//   }
//   mat->makeCompressed();
// }


} // namespace PS
