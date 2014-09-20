#pragma once
#include "base/shared_array_inl.h"
#include "base/sparse_matrix.h"
#include "util/parallel_sort.h"

namespace PS {

template<typename I, typename V>
class Localizer {
 public:
  Localizer(const MatrixPtr<V>& mat) {
    if (mat) mat_ = std::static_pointer_cast<SparseMatrix<I, V>>(mat);
  }

  // return ordered unique indeces with their number of occrus
  void countUniqIndex(SArray<I>* uniq_idx, SArray<uint32>* idx_frq = nullptr);

  // return a matrix with index mapped: idx_dict[i] -> i. Any index does not exists
  // in *idx_dict* is dropped. Assume *idx_dict* is ordered
  MatrixPtr<V> remapIndex(const SArray<I>& idx_dict) const;

 private:
  struct Pair {
    I k; size_t i;
  };
  SArray<Pair> pair_;
  SparseMatrixPtr<Key, V> mat_;

};

template<typename I, typename V>
void Localizer<I,V>::countUniqIndex(SArray<I>* uniq_idx, SArray<uint32>* idx_frq) {
  if (!mat_ || mat_->index().empty()) return;
  int num_threads = FLAGS_num_threads; CHECK_GT(num_threads, 0);
  CHECK(uniq_idx);

  auto idx = mat_->index();
  CHECK_EQ(idx.size(), mat_->offset().back());
  pair_.resize(idx.size());
  for (size_t i = 0; i < idx.size(); ++i) {
    pair_[i].k = idx[i];
    pair_[i].i = i;
  }
  parallelSort(&pair_, FLAGS_num_threads, [](const Pair& a, const Pair& b) {
      return a.k < b.k; });

  uniq_idx->clear();
  if (idx_frq) idx_frq->clear();

  I curr = pair_[0].k;
  uint32 cnt = 0;
  for (const Pair& v : pair_) {
    if (v.k != curr) {
      uniq_idx->pushBack(curr);
      curr = v.k;
      if (idx_frq) idx_frq->pushBack(cnt);
      cnt = 0;
    }
    ++ cnt;
  }
  uniq_idx->pushBack(curr);
  if (idx_frq) idx_frq->pushBack(cnt);

  // for debug
  // index_.writeToFile("index");
  // uniq_idx->writeToFile("uniq");
  // idx_frq->writeToFile("cnt");
}

template<typename I, typename V>
MatrixPtr<V> Localizer<I,V>::remapIndex(const SArray<I>& idx_dict) const {
  CHECK_LT(idx_dict.size(), kuint32max);
  if (!mat_ || mat_->index().empty() || idx_dict.empty()) {
    return MatrixPtr<V>();
  }

  // TODO multi-thread
  uint32 matched = 0;
  SArray<uint32> remapped_idx(pair_.size(), 0);
  const I* cur_dict = idx_dict.begin();
  const Pair* cur_pair = pair_.begin();
  while (cur_dict != idx_dict.end() && cur_pair != pair_.end()) {
    if (*cur_dict < cur_pair->k) {
      ++ cur_dict;
    } else {
      if (*cur_dict == cur_pair->k) {
        remapped_idx[cur_pair->i] = (uint32)(cur_dict-idx_dict.begin()) + 1;
        ++ matched;
      }
      ++ cur_pair;
    }
  }

  // construct the new matrix
  bool bin = mat_->binary();
  auto offset = mat_->offset();
  auto value = mat_->value();
  SArray<uint32> new_index(matched);
  SArray<size_t> new_offset(offset.size()); new_offset[0] = 0;
  SArray<V> new_value(std::min(value.size(), (size_t)matched));

  CHECK_EQ(offset.back(), remapped_idx.size());
  size_t k = 0;
  for (size_t i = 0; i < offset.size() - 1; ++i) {
    size_t n = 0;
    for (size_t j = offset[i]; j < offset[i+1]; ++j) {
      if (remapped_idx[j] == 0) continue;
      ++ n;
      if (!bin) new_value[k] = value[j];
      new_index[k++] = remapped_idx[j] - 1;
    }
    new_offset[i+1] = new_offset[i] + n;
  }
  CHECK_EQ(k, matched);

  auto info = mat_->info();
  info.set_sizeof_index(sizeof(uint32));
  info.set_nnz(new_index.size());
  info.clear_ins_info();
  SizeR local(0, idx_dict.size());
  if (mat_->rowMajor()) {
    local.to(info.mutable_col());
  } else {
    local.to(info.mutable_row());
  }
  // LL << curr_o << " " << local.end() << " " << curr_j;
  return MatrixPtr<V>(new SparseMatrix<uint32, V>(info, new_offset, new_index, new_value));
}

} // namespace PS
