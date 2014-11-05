
namespace PS {
namespace PARSA {

void DblinkArray::init(const sdt::vector<int>& data, , int cache_limit)) {
  // sort the values
  int n = data.size();
  typedef std::pair<int, int> C;
  std::vector<C> datapair(n);
  for (int i = 0; i < n; ++i) {
    datapair[i].first = i;
    datapair[i].second = i;
  }
  std::sort(datapair.begin(), datapair.end(), [](const C& a, const C&b){
      return a.second < b.second;
    });

  // fill in values
  data_.resize(n);
  for (int i = 0; i < n; ++i) {
    int j = datapair[i].first;
    data_[j].value = datapair[i].second;
    if (i < n - 1) data_[j].next = datapair[i+1].first;
    if (i > 0) data_[j].prev = datapair[i-1].first;
  }

  // init the cache positions
  cache_limit_ = std::max(std::min(cache_limit, data_.back().value), 1);
  cached_pos_.resize(cache_limit_);

  for (int i = cache_limit_-1, k = n; i >= 0; --i) {
    while (k > 0 && datapair[k-1].second >= i) --k;
    cached_pos_[i] = k == n ? -1 : datapair[k].first;
  }
}

void DblinkArray::remove(int i) {
  // soft delete
  const auto& e = data_[i];
  if (e.prev >= 0) data_[e.prev].next = e.next;
  if (e.next >= 0) data_[e.next].prev = e.prev;

  // update cache
  for (int v = min(e.value, cache_limit_-1); v >= 0 && cached_pos_[v] == i; --v) {
    cached_pos_[v] = e.next;
  }
}

void DblinkArray::decrAndSort(int i) {
  // update
  int new_v = --data_[i].value;
  // find the new position
  int j = -1;
  if (new_v < cache_limit_) {
    j = cached_pos_[new_v];
    if (new_v + 1 < cache_limit && cached_pos_[new_v+1] == i) {
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
  // update cache

  if (new_v < cache_limit) {
    for (; new_v > 0 && cached_pos_[new_v] == j; -- new_v) {
      cached_pos_[new_v] = i;
    }
  }
}

} // namespace PARSA
} // namespace PS
