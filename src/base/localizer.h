#pragma once
#include "base/shared_array_inl.h"
#include "base/sparse_matrix.h"
#include "util/parallel_sort.h"
#include "data/slot_reader.h"

namespace PS {

template<typename I>
class Localizer {
 public:
  // Localizer() { LL << "xx"; }
  // explicit Localizer(const Localizer& loc) {
  //   LL << "yy";
  //   pair_ = loc.pair_;
  // }
  // ~Localizer() { LL << pair_.size(); }

  // find the unique indeces with their number of occrus in *idx*
  void countUniqIndex(
      const SArray<I>& idx, SArray<I>* uniq_idx, SArray<uint32>* idx_frq = nullptr);

  // return a matrix with index mapped: idx_dict[i] -> i. Any index does not exists
  // in *idx_dict* is dropped. Assume *idx_dict* is ordered
  template<typename V>
  MatrixPtr<V> remapIndex(
      const SlotReader& reader, int grp_id, const SArray<I>& idx_dict) const;

  template<typename V>
  MatrixPtr<V> remapIndex(
      const MatrixInfo& info, const SArray<size_t>& offset,
      const SArray<I>& index, const SArray<V>& value,
      const SArray<I>& idx_dict) const;
  void clear() { pair_.clear(); }
 private:
#pragma pack(4)
  struct Pair {
    I k; uint32 i;
  };
  SArray<Pair> pair_;
};

template<typename I> void Localizer<I>::countUniqIndex(
    const SArray<I>& idx, SArray<I>* uniq_idx, SArray<uint32>* idx_frq) {
  if (idx.empty()) return;
  CHECK(uniq_idx);
  CHECK_LT(idx.size(), kuint32max)
      << "well, you need to change Pair.i from uint32 to uint64";
  CHECK_GT(FLAGS_num_threads, 0);

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

template<typename I>
template<typename V>
MatrixPtr<V> Localizer<I>::remapIndex(
    const SlotReader& reader, int grp_id, const SArray<I>& idx_dict) const {
  SArray<V> val;
  auto info = reader.info<V>(grp_id);
  CHECK_NE(info.type(), MatrixInfo::DENSE)
      << "dense matrix already have compact indeces\n" << info.DebugString();
  if (info.type() == MatrixInfo::SPARSE) val = reader.value<V>(grp_id);
  return remapIndex(info, reader.offset(grp_id), reader.index(grp_id), val, idx_dict);
}

template<typename I>
template<typename V>
MatrixPtr<V> Localizer<I>::remapIndex(
    const MatrixInfo& info, const SArray<size_t>& offset,
    const SArray<I>& index, const SArray<V>& value,
    const SArray<I>& idx_dict) const {
  if (index.empty() || idx_dict.empty()) return MatrixPtr<V>();
  CHECK_LT(idx_dict.size(), kuint32max);
  CHECK_EQ(offset.back(), index.size());
  CHECK_EQ(index.size(), pair_.size());
  bool bin = value.empty();
  if (!bin) CHECK_EQ(value.size(), index.size());

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
  SArray<uint32> new_index(matched);
  SArray<size_t> new_offset(offset.size()); new_offset[0] = 0;
  SArray<V> new_value(std::min(value.size(), (size_t)matched));

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

  auto new_info = info;
  new_info.set_sizeof_index(sizeof(uint32));
  new_info.set_nnz(new_index.size());
  new_info.clear_ins_info();
  SizeR local(0, idx_dict.size());
  if (new_info.row_major())  {
    local.to(new_info.mutable_col());
  } else {
    local.to(new_info.mutable_row());
  }
  // LL << curr_o << " " << local.end() << " " << curr_j;
  return MatrixPtr<V>(new SparseMatrix<uint32, V>(new_info, new_offset, new_index, new_value));
}

} // namespace PS
