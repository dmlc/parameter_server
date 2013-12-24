#pragma once
#include "util/common.h"

namespace PS {
// only Insert and Erase are thread safe
template <typename T>
class FuturePool {
 public:
  FuturePool() : min_key_(kint32max) { }
  // wait up to key (including it) has been set values, only the exisiting keys
  // are considered
  void WaitUntil(int32 key) const;
  // insert a key if not exist, and return the future.
  void Insert(int32 key, std::shared_future<T> *fut = NULL);
  // erease the key
  void Erase(int32 key);
  // set the value of the key to promise
  void Set(int32 key, T v);
  std::shared_future<T> GetFuture(int32 key);
  bool Has(int32 key) const;
  int32 min_key() const { return min_key_; }
private:
  std::mutex mutex_;
  map<int32, std::shared_future<T> > futures_;
  map<int32, std::promise<T> > promises_;
  int32 min_key_;
};

template <typename T>
bool FuturePool<T>::Has(int32 key) const {
  bool has = (promises_.find(key) != promises_.end());
  CHECK_EQ(has, futures_.find(key) != futures_.end());
  return has;
}


template <typename T>
std::shared_future<T> FuturePool<T>::GetFuture(int32 key) {
  CHECK(Has(key));
  return futures_[key];
}

template <typename T>
void FuturePool<T>::Insert(int32 key, std::shared_future<T> *fut) {
  std::lock_guard<std::mutex> guard(mutex_);
  if (!Has(key)) {
    promises_[key] = std::promise<T>(std::allocator_arg,std::allocator<T>());
    futures_[key] = promises_[key].get_future();
    min_key_ = std::min(key, min_key_);
  }
  if (fut != NULL)
    *fut = futures_[key];
}

template <typename T>
void FuturePool<T>::Erase(int32 key) {
  std::lock_guard<std::mutex> guard(mutex_);
  if (!Has(key)) return;
  promises_.erase(key);
  futures_.erase(key);
  for (auto& i : promises_) {
    min_key_ = std::min(min_key_, i.first);
  }
}

template <typename T>
void FuturePool<T>::WaitUntil(int32 key) const {
  if (key < min_key_) return;
  for (auto& it : futures_)
    if (it.first <= key)
      it.second.wait();
}

template <typename T>
void FuturePool<T>::Set(int32 key, T v) {
  if (!Has(key))
    Insert(key, NULL);
  // TODO catch exception
  promises_[key].set_value(v);
}

} // namespace PS
