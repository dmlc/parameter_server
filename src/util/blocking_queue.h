#pragma once

#include <algorithm>
#include <deque>

#include "util/lock.h"

namespace PS {

// thread safe bloking queue
template <class E>
class BlockingQueue {
 public:
  BlockingQueue();
  // put an element into the tail
  void Put(const E& e);
  // take an element from the head. if the queue is empty, then wait
  E Take();
  // try to take an element from the front without waiting. return 1 if taking
  // is sucessful, retrun false otherwise
  bool TryTake(E *e);
  int Size() const;
  bool Empty() const;

 private:
  // The queue of elements. Deque is used to provide O(1) time
  // for head elements removal.
  std::deque<E> queue_;
  // The mutex used for queue synchronization.
  mutable Mutex mutex_;
  // The conditionial variable associated with the mutex above.
  mutable CondVar cond_var_;
};

template<class E>
BlockingQueue<E>::BlockingQueue()
    : cond_var_(&mutex_) {
}

template<class E>
int BlockingQueue<E>::Size() const {
  ScopeLock lock(&mutex_);
  return queue_.size();
}

template<class E>
bool BlockingQueue<E>::Empty() const {
  ScopeLock lock(&mutex_);
  return queue_.empty();
}

template<class E>
void BlockingQueue<E>::Put(const E& e) {
  ScopeLock lock(&mutex_);
  queue_.push_back(e);
  cond_var_.Signal();
}

template<class E>
E BlockingQueue<E>::Take() {
  ScopeLock lock(&mutex_);
  while (queue_.empty()) {
    cond_var_.Wait();
  }
  E e = queue_.front();
  queue_.pop_front();
  return e;
}

template<class E>
bool BlockingQueue<E>::TryTake(E* e) {
  ScopeLock lock(&mutex_);
  if (queue_.empty()) {
    return false;
  }
  *e = queue_.front();
  queue_.pop_front();
  return true;
}

}
