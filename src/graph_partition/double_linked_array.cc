#include "graph_partition/double_linked_array.h"
namespace PS {
namespace PARSA {

void DblinkArray::init(const std::vector<int>& data, int cache_limit) {
  // sort the values
  int n = data.size();
  typedef std::pair<int, int> C;
  std::vector<C> datapair(n);
  for (int i = 0; i < n; ++i) {
    datapair[i].first = i;
    datapair[i].second = data[i];
  }
  std::sort(datapair.begin(), datapair.end(), [](const C& a, const C&b){
      return a.second < b.second;
    });

  // fill in values
  size_ = n;
  data_.resize(n);
  for (int i = 0; i < n; ++i) {
    int j = datapair[i].first;
    data_[j].value = datapair[i].second;
    data_[j].next = i < n - 1 ? datapair[i+1].first : -1;
    data_[j].prev = i > 0 ? datapair[i-1].first : -1;
  }

  // init the cache positions
  cache_limit_ = std::max(std::min(cache_limit, datapair.back().second), 1);
  cached_pos_.resize(cache_limit_);

  for (int i = cache_limit_-1, k = n; i >= 0; --i) {
    while (k > 0 && datapair[k-1].second >= i) --k;
    cached_pos_[i] = k == n ? -1 : datapair[k].first;
  }

  // check();
}

void DblinkArray::remove(int i) {
  CHECK_LT(i, data_.size());
  CHECK_GE(i, 0);

  // soft delete
  const auto& e = data_[i];
  if (e.prev >= 0) data_[e.prev].next = e.next;
  if (e.next >= 0) data_[e.next].prev = e.prev;
  --size_;

  // update cache
  for (int v = std::min(e.value, cache_limit_-1); v >= 0 && cached_pos_[v] == i; --v) {
    cached_pos_[v] = e.next;
  }

  // check();
}

void DblinkArray::check() {
  // check cache
  if (size_ == 0) return;
  for (int i = 0; i < cache_limit_; ++i) {
    int p = cached_pos_[i];
    if (p >= 0) CHECK_GE(data_[p].value, i);
  }
  int j = 0;
  int p = cached_pos_[0];
  CHECK_EQ(data_[p].prev, -1);
  while (true) {
    int q = data_[p].next;
    if (q == -1) break;
    ++ j; CHECK_LT(j, size_);
    CHECK_LE(data_[p].value, data_[p].value);
    CHECK_EQ(p, data_[q].prev);
    p = q;
  }
  CHECK_EQ(j, size_ - 1);
}

void DblinkArray::decrAndSort(int i) {
  // update
  int new_v = --data_[i].value; CHECK_GE(new_v, 0);
  // find the new position
  int j = -1;
  if (new_v < cache_limit_) {
    j = cached_pos_[new_v];
    if (new_v + 1 < cache_limit_ && cached_pos_[new_v+1] == i) {
      cached_pos_[new_v+1] = data_[i].next;
    }
  } else {
    j = i;
    int k = 0, n = data_.size();
    for (; k < n; ++k) {
      int prev = data_[j].prev;
      if (prev == -1 || data_[prev].value <= new_v) break;
      j = prev;
    }
    CHECK_NE(k, n) << "detect a loop";
  }
  // do nothing
  if (j == i) return;
  // remove i,
  remove(i);
  // insert i before j
  int prev = data_[j].prev;
  if (prev >= 0) data_[prev].next = i;
  data_[j].prev = i;
  data_[i].next = j;
  data_[i].prev = prev;
  ++ size_;
  // update cache

  new_v = std::min(new_v, cache_limit_-1);
  for (; new_v >= 0 && cached_pos_[new_v] == j; -- new_v) {
    cached_pos_[new_v] = i;
  }


  // check();
}

} // namespace PARSA
} // namespace PS
