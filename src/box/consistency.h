#pragma once
#include "util/common.h"
#include "util/status.h"
#include "util/futurepool.h"
#include "proto/header.pb.h"

namespace PS {

// the time stamp
typedef int32 time_t;

// the sender do a request, and will be blocked if the consisteny requirement is
// not satisfied.
template <typename T>
class Consistency {
 public:
  typedef std::shared_future<T> Future;
  // a large value of delay
  static const time_t kInfDelay = kint32max;
  // in default, best effort, i.e. eventual consistency
  Consistency() { SetMaxDelay(kInfDelay, kInfDelay); }
  // Consistency(time_t max_push_delay, time_t max_pull_delay);
  inline void SetMaxDelay(time_t push, time_t pull);
  // block until the consistency requirement is satisifed again after inserting
  // this request
  inline void Request(Header *flag, Future* push_fut=NULL, Future* pull_fut=NULL);
  inline void ResponsePush(const Header& flag, T v);
  inline void ResponsePull(const Header& flag, T v);
 private:
  time_t max_push_delay_;
  time_t max_pull_delay_;
  // TODO replace bool with some statistic information about sending and receiving
  FuturePool<T> push_pool_;
  FuturePool<T> pull_pool_;
};

template <typename T>
void Consistency<T>::Request(Header *flag, Future* push_fut, Future* pull_fut) {
  time_t t = flag->time();
  if (flag->type() & Header::PUSH) {
    push_pool_.Insert(t, push_fut);
    push_pool_.WaitUntil(t-max_push_delay_);
  }
  if (flag->type() & Header::PULL) {
    pull_pool_.Insert(t, pull_fut);
    pull_pool_.WaitUntil(t-max_pull_delay_);
  }
}

template <typename T>
void Consistency<T>::ResponsePush(const Header& flag, T v) {
  time_t t = flag.time();
  if (flag.type() & Header::PUSH) {
    push_pool_.Set(t,v);
    return;
  }
}

template <typename T>
void Consistency<T>::ResponsePull(const Header& flag, T v) {
  time_t t = flag.time();
  if (flag.type() & Header::REPLY) {
    pull_pool_.Set(t, v);
  }
}

template <typename T>
void Consistency<T>::SetMaxDelay(time_t push, time_t pull) {
  max_push_delay_ = std::max(push, 0);
  max_pull_delay_ = std::max(pull, 0);
}

} // namespace PS
