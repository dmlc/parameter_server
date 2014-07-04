#pragma once

#include <thread>
#include <sparsehash/dense_hash_set>
#include <sparsehash/dense_hash_map>

#include "util/common.h"
#include "util/threadpool.h"
#include "base/matrix.h"
#include "base/shared_array.h"
#include "base/range.h"

namespace PS {

// sparse matrix with Yale format
template<typename I, typename V>
class SparseMatrix : public Matrix<V> {
 public:
  SparseMatrix(
      const MatrixInfo& info, SArray<size_t> offset, SArray<I> index, SArray<V> value)
      : Matrix<V>(info, value), offset_(offset), index_(index) { }

  bool binary() const { return this->info_.type() == MatrixInfo::SPARSE_BINARY; }

  void times(const double* x, double *y) const { templateTimes(x, y); }
  MatrixPtr<V> dotTimes(const MatrixPtr<V>& B) const;

  // (nearly) non-copy matrix transpose
  MatrixPtr<V> trans() const {
    auto B = new SparseMatrix<I,V>(*this);
    B->tranposeInfo();
    return MatrixPtr<V>(B);
  }

  MatrixPtr<V> colBlock(SizeR range) const;
  MatrixPtr<V> rowBlock(SizeR range) const;

  MatrixPtr<V> alterStorage() const;

  MatrixPtr<V> localize(SArray<Key>* key_map) const {
    I max_key = rowMajor() ? info_.col().end() : info_.row().end();
    return (max_key > (I)kuint32max ?
            localizeBigKey(key_map) : localizeSmallKey(key_map));
  }

  MatrixPtr<V> localizeBigKey(SArray<Key>* key_map) const;
  MatrixPtr<V> localizeSmallKey(SArray<Key>* key_map) const;

  // debug string
  string debugString() const;

  bool writeToBinFile(string name) const {
    return (WriteProtoToASCIIFile(info_, name+".info") &&
            offset_.writeToFile(name+".offset") &&
            index_.writeToFile(name+".index") &&
            (binary() || value_.writeToFile(name+".value")));
  }


  SArray<I> index() const { return index_; }
  SArray<size_t> offset() const { return offset_; }

  using Matrix<V>::rows;
  using Matrix<V>::cols;
  using Matrix<V>::nnz;
 private:
  using Matrix<V>::info_;
  using Matrix<V>::value_;
  using Matrix<V>::rowMajor;
  using Matrix<V>::empty;
  using Matrix<V>::innerSize;
  using Matrix<V>::outerSize;

  //// y = A * x, version 1. simper and faster
  // single-thread,  both x and y are pre-allocated
  template <typename W>
  void rangeTimes(SizeR row_range, const W* const x, W* y) const {
    if (rowMajor()) {
      for (size_t i = row_range.begin(); i < row_range.end(); ++i) {
        if (offset_[i] == offset_[i+1]) continue;
        W y_i = 0;
        if (binary()) {
          for (size_t j = offset_[i]; j < offset_[i+1]; ++j)
            y_i += x[index_[j]];
        } else {
          for (size_t j = offset_[i]; j < offset_[i+1]; ++j)
            y_i += x[index_[j]] * value_[j];
        }
        y[i] = y_i;
      }
    } else {
      memset(y + row_range.begin(), 0, sizeof(W) * row_range.size());
      for (size_t i = 0; i < offset_.size() - 1; ++i) {
        if (offset_[i] == offset_[i+1]) continue;
        W w_i = x[i];
        if (binary()) {
          for (size_t j = offset_[i]; j < offset_[i+1]; ++j) {
            auto k = index_[j];
            if (row_range.contains(k))
              y[k] += w_i;
          }
        } else {
          for (size_t j = offset_[i]; j < offset_[i+1]; ++j) {
            auto k = index_[j];
            if (row_range.contains(k))
              y[k] += w_i * value_[j];
          }
        }
      }
    }
  }

  // multi-thread,  both x and y are pre-allocated
  template <typename W>
  void templateTimes(const W* x, W* y) const {
    SizeR row_range(0, rows());
    int num_threads = FLAGS_num_threads;
    CHECK_GT(num_threads, 0);

    ThreadPool pool(num_threads);
    int num_tasks = rowMajor() ? num_threads * 10 : num_threads;
    for (int i = 0; i < num_tasks; ++i) {
      pool.add([this, x, y, row_range, num_tasks, i](){
          rangeTimes(row_range.evenDivide(num_tasks, i), x, y);
        });
    }
    pool.startWorkers();
  }

 private:
  SArray<size_t> offset_;
  SArray<I> index_;
};


template<typename I, typename V>
MatrixPtr<V> SparseMatrix<I,V>::dotTimes(const MatrixPtr<V>& B) const {
  auto C = std::static_pointer_cast<const SparseMatrix<I,V> >(B);
  CHECK_EQ(rows(), C->rows());
  CHECK_EQ(cols(), C->cols());
  CHECK_EQ(nnz(), C->nnz());  // TODO limited support yet...

  SArray<V> new_value;
  if (binary()) {
    new_value = C->value_;
  } else if (C->binary()) {
    new_value = value_;
  } else {
    new_value.resize(value_.size());
    for (size_t i = 0; i < value_.size(); ++i)
      new_value[i] = value_[i] * C->value_[i];
  }
  return MatrixPtr<V>(new SparseMatrix<I,V>(info_, offset_, index_, new_value));
}

template<typename I, typename V>
MatrixPtr<V> SparseMatrix<I,V>::colBlock(SizeR range) const {
  // CHECK(!range.empty());
  CHECK(range.valid());
  if (rowMajor()) {
    CHECK_EQ(range.size(), cols()) << "limited support yet";
    return MatrixPtr<V>(new SparseMatrix<I,V>(info_, offset_, index_, value_));
  } else {
    // if (range.empty()) LL << range;
    auto new_offset = offset_.segment(SizeR(range.begin(), range.end()+1));
    auto new_info = info_;
    range.to(new_info.mutable_col());
    new_info.set_nnz(new_offset.back() - new_offset.front());
    return MatrixPtr<V>(new SparseMatrix<I,V>(new_info, new_offset, index_, value_));
  }

}

template<typename I, typename V>
MatrixPtr<V> SparseMatrix<I,V>::rowBlock(SizeR range) const {
  CHECK(rowMajor());
  CHECK(!range.empty());

  auto new_offset = offset_.segment(SizeR(range.begin(), range.end()+1));

  auto new_info = info_;
  range.to(new_info.mutable_row());
  new_info.set_nnz(new_offset.back() - new_offset.front());

  return MatrixPtr<V>(new SparseMatrix<I,V>(new_info, new_offset, index_, value_));
  // return static_cast<Matrix>(re);
}

template<typename I, typename V>
MatrixPtr<V> SparseMatrix<I,V>::alterStorage() const {
  CHECK(!empty());

  // build the new offset
  size_t inner_n = this->innerSize();
  CHECK_LT(inner_n, (size_t)kuint32max) << "run localize first";

  SArray<size_t> new_offset(inner_n + 1);
  new_offset.setZero();

  int num_threads = FLAGS_num_threads;
  CHECK_GT(num_threads, 0);
  {
    ThreadPool pool(num_threads);
    for (int i = 0; i < num_threads; ++i) {
      SizeR range = SizeR(0, inner_n).evenDivide(num_threads, i);
      pool.add([this, range, &new_offset](){
          for (I k : index_) if (range.contains(k)) ++ new_offset[k+1];
        });
    }
    pool.startWorkers();
  }
  for (size_t i = 0; i < inner_n; ++i) {
    new_offset[i+1] += new_offset[i];
  }

  // fill in index and value
  SArray<I> new_index(index_.size());
  SArray<V> new_value(value_.size());
  {
    ThreadPool pool(num_threads);
    for (int i = 0; i < num_threads; ++i) {
      SizeR range = SizeR(0, inner_n).evenDivide(num_threads, i);
      pool.add([this, range, &new_offset, &new_value, &new_index](){
          for (size_t i = 0; i < outerSize(); ++i) {
            if (offset_[i] == offset_[i+1]) continue;
            for (size_t j = offset_[i]; j < offset_[i+1]; ++j) {
              I k = index_[j];
              if (!range.contains(k)) continue;
              if (!binary()) new_value[new_offset[k]] = value_[j];
              new_index[new_offset[k]++] = static_cast<I>(i);
            }
          }
        });
    }
    pool.startWorkers();
  }
  for (size_t i = inner_n -1; i > 0; --i)
    new_offset[i] = new_offset[i-1];
  new_offset[0] = 0;

  auto new_info = info_;
  new_info.set_row_major(!info_.row_major());

  return MatrixPtr<V>(new SparseMatrix<I,V>(new_info, new_offset, new_index, new_value));
}

template<typename I, typename V>
MatrixPtr<V> SparseMatrix<I,V>::localizeSmallKey(SArray<Key>* key_map) const {
  int num_threads = FLAGS_num_threads;
  CHECK_GT(num_threads, 0);

  I inner_end = rowMajor() ? info_.col().end() : info_.row().end();
  I bucket = (inner_end-1) / num_threads + 1;

  std::vector<uint32> map(bucket*num_threads); // global to local map

  // find unique keys
  std::vector<I> nnz(num_threads+1);
  {
    ThreadPool pool(num_threads);
    for (int i = 0; i < num_threads; ++i) {
      pool.add([this, i, bucket, &map, &nnz](){
          Range<I> range(bucket*i, bucket*(i+1));
          for (I k : index_) if (range.contains(k)) map[k] = -1;
          Key z = 0;
          for (I j = range.begin(); j < range.end(); ++j)
            if (map[j] == -1) ++ z;
          nnz[i+1] = z;
        });
    }
    pool.startWorkers();
  }

  nnz[0] = 0; for (int i = 0; i < num_threads; ++i) nnz[i+1] += nnz[i];

  key_map->resize(nnz[num_threads]);
  SArray<uint32> new_index (index_.size());
  {
    ThreadPool pool(num_threads);
    for (int i = 0; i < num_threads; ++i) {
      pool.add([this, i, bucket, key_map, &nnz, &map, &new_index]() {
          // construct the key map
          uint32 local_key = nnz[i];
          Range<I> range(bucket*i, std::min(bucket*(i+1), (I)map.size()));
          for (I j = range.begin(); j < range.end(); ++j) {
            if (map[j] != -1) continue;
            map[j] = local_key;
            (*key_map)[local_key++] = static_cast<Key>(j);
          }
          // remap index
          for (size_t j = 0; j < index_.size(); ++j) {
            I k = index_[j];
            if (range.contains(k)) new_index[j] = map[k];
          }
        });
    }
    pool.startWorkers();
  }

  auto info = info_;
  SizeR local(0, key_map->size());
  if (rowMajor())
    local.to(info.mutable_col());
  else
    local.to(info.mutable_row());

  return MatrixPtr<V>(new SparseMatrix<uint32, V>(info, offset_, new_index, value_));
}

template<typename I, typename V>
MatrixPtr<V> SparseMatrix<I,V>::localizeBigKey(SArray<Key>* key_map) const {
  int num_threads = FLAGS_num_threads; CHECK_GT(num_threads, 0);
  // use a large constant, e.g. 6, here. because dense_hash_set/map may have
  // serious performace issues after inserting too many keys
  int npart = num_threads * 6;
  // int npart = 20;

  auto range = rowMajor() ? Range<I>(info_.col()) : Range<I>(info_.row());

  // std::vector<std::unordered_set<I>> uniq_keys(npart);
  std::vector<google::dense_hash_set<I>> uniq_keys(npart);

  // find unique keys
  {
    ThreadPool pool(num_threads);
    for (int i = 0; i < npart; ++i) {
      auto thread_range = range.evenDivide(npart, i);
      pool.add([this, i, thread_range, &uniq_keys](){
          auto& uk = uniq_keys[i];
          uk.set_empty_key(-1);
          // size_t n = 0;
          for (I k : index_) if (thread_range.contains(k)) {uk.insert(k);}
          // LL << n << " " << thread_range;
        });
    }
    pool.startWorkers();
  }

  std::vector<I> nnz(npart+1);
  nnz[0] = 0;
  for (int i = 0; i < npart; ++i) nnz[i+1] += nnz[i] + uniq_keys[i].size();

  key_map->resize(nnz[npart]);
  SArray<uint32> new_index (index_.size());
  {
    ThreadPool pool(num_threads);
    for (int i = 0; i < npart; ++i) {
      auto thread_range = range.evenDivide(npart, i);
      pool.add([this, i, thread_range, key_map, &nnz, &uniq_keys, &new_index]() {
          // order the unique global keys
          auto& uk = uniq_keys[i];
          std::vector<I> ordered_keys(uk.size());
          size_t j = 0;
          for (auto it = uk.begin(); it != uk.end(); ++it) ordered_keys[j++] = *it;
          std::sort(ordered_keys.begin(), ordered_keys.end());
          uk.clear();

          // construct the key map
          uint32 local_key = nnz[i];
          // std::unordered_map<I, uint32> map;
          google::dense_hash_map<I, uint32> map;
          map.set_empty_key(-1);

          for (uint32 i = 0; i < ordered_keys.size(); ++i) {
            auto key = ordered_keys[i];
            map[key] = local_key;
            (*key_map)[local_key++] = static_cast<Key>(key);
          }

          // remap index
          for (size_t j = 0; j < index_.size(); ++j) {
            I k = index_[j];
            if (thread_range.contains(k)) new_index[j] = map[k];
          }
        });
    }
    pool.startWorkers();
  }

  auto info = info_;
  SizeR local(0, key_map->size());
  if (rowMajor())
    local.to(info.mutable_col());
  else
    local.to(info.mutable_row());

  return MatrixPtr<V>(new SparseMatrix<uint32, V>(info, offset_, new_index, value_));
}

// debug
template<typename I, typename V>
std::ostream& operator<<(std::ostream& os, const SparseMatrix<I,V>& mat) {
  os << mat.debugString();
  return os;
}


template<typename I, typename V>
string SparseMatrix<I,V>::debugString() const {
  std::stringstream ss;
  int nnz = offset_.back() - offset_.front();
  ss << info_.DebugString() << "offset: " << offset_ << "\n"
     << "index: " << dbstr(index_.data()+offset_[0], nnz)  << "\n"
     << "value: " << dbstr(value_.data()+offset_[0], nnz)  << "\n";
  return ss.str();
}

} // namespace PS


  //// version 2
  // // single-thread y = A * x, both x and y are pre-allocated
  // template <typename W>
  // void timesRowMajor(SizeR row_range, const W* const x, W* y) {
  //   for (size_t i = row_range.begin(); i < row_range.end(); ++i) {
  //     if (offset_[i] == offset_[i+1]) continue;
  //     W val = 0;
  //     if (binary()) {
  //       for (size_t j = offset_[i]; j < offset_[i+1]; ++j)
  //         val += x[index_[j]];
  //     } else {
  //       for (size_t j = offset_[i]; j < offset_[i+1]; ++j)
  //         val += x[index_[j]] * value_[j];
  //     }
  //     y[i-offset_[0]] = val;
  //   }
  // }

  // template <typename W>
  // void timesColMajor(SizeR col_range, const W* const x, W* y) {
  //   memset(y, 0,  sizeof(W) * rows());
  //   for (size_t i = col_range.begin(); i < col_range.end(); ++i) {
  //     if (offset_[i] == offset_[i+1]) continue;
  //     W val = x[i-offset_[0]];
  //     if (binary()) {
  //       for (size_t j = offset_[i]; j < offset_[i+1]; ++j)
  //         y[index_[j]] += val;
  //     } else {
  //       for (size_t j = offset_[i]; j < offset_[i+1]; ++j)
  //         y[index_[j]] += val * value_[j];
  //     }
  //   }
  // }

  // // multi-thread y = A * x, both x and y are pre-allocated
  // template <typename W>
  // void times(const W* const x, W* y) {
  //   int num_threads = FLAGS_num_threads;
  //   CHECK_GT(num_threads, 0);
  //   int npart = num_threads;

  //   std::vector<SArray<W>> ys;
  //   {
  //     ThreadPool pool(num_threads);
  //     if (rowMajor()) {
  //       npart *= 10;
  //       SizeR range(0, rows());
  //       for (int i = 0; i < npart; ++i) {
  //         pool.Add([this, x, y, range, npart, i](){
  //             timesRowMajor(range.evenDivide(npart, i), x, y);
  //           });
  //       }
  //     } else {
  //       SizeR range(0, cols());
  //       for (int i = 0; i < npart; ++i) {
  //         W* yp = y;
  //         if (i > 0) {
  //           ys.push_back(SArray<W>(rows()));
  //           yp = ys.back().data();
  //         }
  //         pool.Add([this, x, yp, range, npart, i](){
  //             timesColMajor(range.evenDivide(npart, i), x, yp);
  //           });
  //       }
  //     }
  //     pool.StartWorkers();
  //   }

  //   if (!rowMajor() && num_threads > 1) {
  //     Eigen::Map<Eigen::Matrix<W, Eigen::Dynamic, 1> > v(y, rows());
  //     for (auto& s : ys) v += s.vec();
  //   }
  // }
