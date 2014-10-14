#pragma once

#include <thread>

#if GOOGLE_HASH
#include <sparsehash/dense_hash_set>
#include <sparsehash/dense_hash_map>
#endif

#include "util/common.h"
#include "util/threadpool.h"
#include "util/parallel_sort.h"
#include "base/matrix.h"
#include "base/shared_array.h"
#include "base/range.h"

namespace PS {
template<typename I, typename V> class SparseMatrix;
template<typename I, typename V>
using SparseMatrixPtr = std::shared_ptr<SparseMatrix<I, V>>;

// sparse matrix with Yale format
template<typename I, typename V>
class SparseMatrix : public Matrix<V> {
 public:
  USING_MATRIX;
  SparseMatrix() { }
  SparseMatrix(
      const MatrixInfo& info, SArray<size_t> offset, SArray<I> index, SArray<V> value)
      : Matrix<V>(info, value), offset_(offset), index_(index) {
  }

  void times(const V* x, V* y) const { templateTimes(x, y); }
  MatrixPtr<V> dotTimes(const MatrixPtr<V>& B) const;

  // (nearly) non-copy matrix transpose
  MatrixPtr<V> trans() const {
    auto B = new SparseMatrix<I,V>(*this);
    B->tranposeInfo();
    return MatrixPtr<V>(B);
  }

  MatrixPtr<V> alterStorage() const;

  // debug string
  string debugString() const;

  bool writeToBinFile(string name) const {
    return (writeProtoToASCIIFile(info_, name+".info") &&
            offset_.writeToFile(name+".offset") &&
            index_.writeToFile(name+".index") &&
            (binary() || value_.writeToFile(name+".value")));
  }

  MatrixPtr<V> colBlock(SizeR range) const;
  MatrixPtr<V> rowBlock(SizeR range) const;

  bool binary() const { return this->info_.type() == MatrixInfo::SPARSE_BINARY; }
  SArray<I> index() const { return index_; }
  SArray<size_t> offset() const { return offset_; }

  size_t memSize() const {
    return value_.memSize() + index_.memSize() + offset_.memSize();
  }
  void resize(size_t rows, size_t cols, size_t nnz, bool row_major) {
    CHECK(false) << "TODO";
  }
 private:

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

// template<typename I, typename V>
// void SparseMatrix<I,V>::countUniqIndex(
//     SArray<I>* uniq_idx, SArray<uint32>* idx_cnt) const {
//   CHECK(!index_.empty());
//   int num_threads = FLAGS_num_threads; CHECK_GT(num_threads, 0);

//   SArray<I> sorted_index;
//   sorted_index.copyFrom(index_);
//   // std::sort(sorted_index.begin(), sorted_index.end());
//   parallelSort(&sorted_index, num_threads);

//   uniq_idx->clear();
//   if (idx_cnt) idx_cnt->clear();

//   I curr = sorted_index[0];
//   uint32 cnt = 0;
//   for (I v : sorted_index) {
//     ++ cnt;
//     if (v != curr) {
//       uniq_idx->pushBack(curr);
//       curr = v;
//       if (idx_cnt) idx_cnt->pushBack(cnt);
//       cnt = 0;
//     }
//   }
//   if (!uniq_idx->empty()) {
//     uniq_idx->pushBack(curr);
//     if (idx_cnt) idx_cnt->pushBack(cnt);
//   }
//   // index_.writeToFile("index");
//   // uniq_idx->writeToFile("uniq");
//   // idx_cnt->writeToFile("cnt");
// }

// template<typename I, typename V>
// MatrixPtr<V> SparseMatrix<I,V>::remapIndex(const SArray<I>& idx_map) const {
//   int num_threads = FLAGS_num_threads; CHECK_GT(num_threads, 0);
//   CHECK_LT(idx_map.size(), kuint32max);

//   // a hashmap solution, often uses 2x times than countUniqIndex
//   // TODO use a countmin-like approach to accelerate this code block.
//   // TODO new_index is too large... not memory efficient
//   SArray<uint32> new_index(index_.size());
//   new_index.setValue(-1);
//   {
//     ThreadPool pool(num_threads);
//     // use a large constant, e.g. 6, here. because dense_hash_set/map may have
//     // serious performace issues after inserting too many keys
//     int npart = num_threads * 4;

//     for (int i = 0; i < npart; ++i) {
//       auto thread_range = idx_map.range().evenDivide(npart, i);
//       pool.add([this, thread_range, &idx_map, &new_index]() {
//           // build the hash map, takes 50% time. the speed of unordered_map is
//           // comparable to google::dense_hash_map (tested in gcc4.8).
//           std::unordered_map<I, uint32> map;
//           auto arr_range = idx_map.findRange(thread_range);
//           auto key_seg = idx_map.segment(arr_range);
//           size_t j = 0;
//           for (I k : key_seg) map[k] = (j++) + arr_range.begin();

//           // remap index, takes 50% time
//           for (size_t j = 0; j < index_.size(); ++j) {
//             I k = index_[j];
//             if (thread_range.contains(k)) {
//               auto it = map.find(k);
//               if (it != map.end()) new_index[j] = it->second;
//             }
//           }
//         });
//     }
//     pool.startWorkers();
//   }

//   // construct the new matrix
//   bool bin = binary();
//   size_t curr_j = 0, curr_o = 0;
//   SArray<size_t> new_offset(offset_.size());
//   SArray<V> new_value(value_.size());
//   new_offset[0] = 0;
//   for (size_t i = 0; i < offset_.size() - 1; ++i) {
//     size_t n = 0;
//     for (size_t j = offset_[i]; j < offset_[i+1]; ++j) {
//       if (new_index[j] == -1) continue;
//       ++ n;
//       if (!bin) new_value[curr_j] = value_[j];
//       new_index[curr_j++] = new_index[j];
//     }
//     if (n) {
//       new_offset[curr_o+1] = new_offset[curr_o] + n;
//       ++ curr_o;
//     }
//   }
//   new_offset.resize(curr_o+1);
//   new_index.resize(curr_j);
//   if (!bin) new_value.resize(curr_j);

//   auto info = info_;
//   info.set_sizeof_index(sizeof(uint32));
//   info.set_nnz(new_index.size());
//   SizeR local(0, idx_map.size());
//   if (rowMajor()) {
//     SizeR(0, curr_o).to(info.mutable_row());
//     local.to(info.mutable_col());
//   } else {
//     SizeR(0, curr_o).to(info.mutable_col());
//     local.to(info.mutable_row());
//   }
//   // LL << curr_o << " " << local.end() << " " << curr_j;
//   return MatrixPtr<V>(
//       new SparseMatrix<uint32, V>(info, new_offset, new_index, new_value));
// }

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
     << "index: " << dbstr(index_.data()+offset_[0], nnz)  << "\n";
  if (!binary()) ss << "value: " << dbstr(value_.data()+offset_[0], nnz)  << "\n";
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



//   // I max_key = rowMajor() ? info_.col().end() : info_.row().end();
//   // return (max_key > (I)kuint32max ?
//   //         localizeBigKey(key_map) : localizeSmallKey(key_map));
//   MatrixPtr<V> localizeBigKey(SArray<Key>* key_map) const;
//   MatrixPtr<V> localizeSmallKey(SArray<Key>* key_map) const;
// template<typename I, typename V>
// MatrixPtr<V> SparseMatrix<I,V>::localizeSmallKey(SArray<Key>* key_map) const {
//   int num_threads = FLAGS_num_threads;
//   CHECK_GT(num_threads, 0);

//   I inner_end = rowMajor() ? info_.col().end() : info_.row().end();
//   I bucket = (inner_end-1) / num_threads + 1;

//   std::vector<uint32> map(bucket*num_threads); // global to local map

//   // find unique keys
//   std::vector<I> nnz(num_threads+1);
//   {
//     ThreadPool pool(num_threads);
//     for (int i = 0; i < num_threads; ++i) {
//       pool.add([this, i, bucket, &map, &nnz](){
//           Range<I> range(bucket*i, bucket*(i+1));
//           for (I k : index_) if (range.contains(k)) map[k] = -1;
//           Key z = 0;
//           for (I j = range.begin(); j < range.end(); ++j)
//             if (map[j] == -1) ++ z;
//           nnz[i+1] = z;
//         });
//     }
//     pool.startWorkers();
//   }

//   nnz[0] = 0; for (int i = 0; i < num_threads; ++i) nnz[i+1] += nnz[i];

//   key_map->resize(nnz[num_threads]);
//   SArray<uint32> new_index (index_.size());
//   {
//     ThreadPool pool(num_threads);
//     for (int i = 0; i < num_threads; ++i) {
//       pool.add([this, i, bucket, key_map, &nnz, &map, &new_index]() {
//           // construct the key map
//           uint32 local_key = nnz[i];
//           Range<I> range(bucket*i, std::min(bucket*(i+1), (I)map.size()));
//           for (I j = range.begin(); j < range.end(); ++j) {
//             if (map[j] != -1) continue;
//             map[j] = local_key;
//             (*key_map)[local_key++] = static_cast<Key>(j);
//           }
//           // remap index
//           for (size_t j = 0; j < index_.size(); ++j) {
//             I k = index_[j];
//             if (range.contains(k)) new_index[j] = map[k];
//           }
//         });
//     }
//     pool.startWorkers();
//   }

//   auto info = info_;
//   info.set_sizeof_index(sizeof(uint32));
//   SizeR local(0, key_map->size());
//   if (rowMajor())
//     local.to(info.mutable_col());
//   else
//     local.to(info.mutable_row());

//   return MatrixPtr<V>(new SparseMatrix<uint32, V>(info, offset_, new_index, value_));
// }

// template<typename I, typename V>
// MatrixPtr<V> SparseMatrix<I,V>::localizeBigKey(SArray<Key>* key_map) const {
//   // int num_threads = FLAGS_num_threads; CHECK_GT(num_threads, 0);

//   SArray<I> tmp(*key_map);
//   CHECK_EQ(sizeof(Key), sizeof(I));
//   countUniqIndex(&tmp);

//   LL << tmp.size();
//   return remapIndex(tmp);
//   // SArray<uint32> new_index(index_.size());
//   // {
//   //   // TODO use a countmin-like approach to accelerate this code block.
//   //   ThreadPool pool(num_threads);
//   //   // use a large constant, e.g. 6, here. because dense_hash_set/map may have
//   //   // serious performace issues after inserting too many keys
//   //   int npart = num_threads * 4;
//   //   // int npart = 20;

//   //   for (int i = 0; i < npart; ++i) {
//   //     auto thread_range = key_map->range().evenDivide(npart, i);
//   //     pool.add([this, thread_range, key_map, &new_index]() {
//   //         // build the hash map, takes 40% time. the speed of unordered_map is
//   //         // comparable to google::dense_hash_map (tested in gcc4.8).
//   //         std::unordered_map<I, uint32> map;
//   //         auto seg = key_map->findRange(thread_range);
//   //         auto key_seg = key_map->segment(seg);
//   //         size_t j = 0;
//   //         for (I k : key_seg) map[k] = (j++) + seg.begin();

//   //         // remap index, takes 40% time
//   //         for (size_t j = 0; j < index_.size(); ++j) {
//   //           I k = index_[j];
//   //           if (thread_range.contains(k)) new_index[j] = map[k];
//   //         }
//   //       });
//   //   }
//   //   pool.startWorkers();
//   // }
//   // // LL << t.getAndRestart();

//   // // construct the new matrix
//   // auto info = info_;
//   // info.set_sizeof_index(sizeof(uint32));
//   // SizeR local(0, key_map->size());
//   // if (rowMajor())
//   //   local.to(info.mutable_col());
//   // else
//   //   local.to(info.mutable_row());

//   // return MatrixPtr<V>(
//   //     new SparseMatrix<uint32, V>(info, offset_, new_index, value_));
// }
