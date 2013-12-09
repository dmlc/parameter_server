#pragma once
#include <list>
#include "util/common.h"
#include "util/lock.h"

namespace PS {

// a thread safe container, each element is associated with a timestamp. The
// timestamp is assumed to be unique and increasing. Operattions will be blocked
// if:
// - any insertation with a timestampe >= t + c, where c is the capacity and t is
// the oldest timestamp existed in this pool.
// - pop from an empty pool

template <class E>
class FreshPool {
 public:
  FreshPool() : capacity_(1), cond_var_(&mutex_) { }

  void set_capacity(int capacity);

  // will be blocked if t >= t_oldest + capacity
  bool PushBack(int t, E e);
  // will be blocked until this pool is not emtpy
  E PopFront();

  // nonblocking call, return true of pop successfully, otherwise return false
  bool TryPopFront(E *e);

  // return faslse if didn't find the element
  bool Erase(int t, E* e=NULL);

  // just for debug use
  string Print() {
    ScopeLock lock(&mutex_);
    if (Empty()) return StrCat("empty/", capacity_);
    return StrCat(pool_.front().first, ",", pool_.back().first, "/", capacity_);
  }

  // E At(int i);
  // bool Insert(int t, E e);

 private:
  // these two are not thread-safe
  bool Empty();
  bool Full();
  int capacity_;
  // TODO simply use std::list, do profile later to check the optimazation space
  typedef std::list<std::pair<int, E> > pool_t;
  pool_t pool_;
  // for thread safety
  mutable Mutex mutex_;
  mutable CondVar cond_var_;
};

template <class E>
void FreshPool<E>::set_capacity(int capacity) {
  ScopeLock lock(&mutex_);
  capacity_ = capacity;
  CHECK_GT(capacity, 0) << "invalid capactiy value";
}

template <class E>
bool FreshPool<E>::Empty() {
  // ScopeLock lock(&mutex_);
  return pool_.empty();
}


template <class E>
bool FreshPool<E>::Full() {
  // ScopeLock lock(&mutex_);
  if (pool_.empty()) return false;
  int itv = pool_.back().first - pool_.front().first + 1;
  return itv >= capacity_;
}

template <class E>
bool FreshPool<E>::PushBack(int t, E e) {
  ScopeLock lock(&mutex_);
  while (Full()) {
    // the lock of mutex_ will be released within Wait()
    cond_var_.Wait();
  }
  pool_.push_back(std::make_pair(t, e));
  cond_var_.SignalAll();
  return true;
}

template <class E>
E FreshPool<E>::PopFront() {
  ScopeLock lock(&mutex_);
  while (Empty()) {
    // the lock of mutex_ will be released within Wait()
    cond_var_.Wait();
  }
  E e = pool_.front().second;
  pool_.pop_front();
  cond_var_.SignalAll();
  return e;
}

template <class E>
bool FreshPool<E>::TryPopFront(E *e) {
  ScopeLock lock(&mutex_);
  if (Empty()) return false;
  *e = pool_.front().second;
  pool_.pop_front();
  cond_var_.SignalAll();
  return true;
}

template <class E>
bool FreshPool<E>::Erase(int t, E* e) {
  ScopeLock lock(&mutex_);
  // do linear search
  typename pool_t::iterator it = pool_.begin();
  for (; it != pool_.end(); ++it) {
    if (it->first == t) {
      if (e) *e = it->second;
      pool_.erase(it);
      cond_var_.SignalAll();
      return true;
    }
  }
  return false;
}

} // namespace PS
