#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include "util/common.h"

namespace PS {

template<typename T>
class threadsafeLimitedQueue {
 public:
  threadsafeLimitedQueue() { }
  void setMaxCapacity(size_t capacity) { max_capacity_ = capacity; }

  void push(const T& value, size_t capacity) {
    if (capacity > max_capacity_) { LL << "you will be blocked here forever..."; }
    std::unique_lock<std::mutex> l(mu_);
    full_cond_.wait(l, [this, capacity]{
        return (capacity + cur_capacity_ <= max_capacity_); });
    queue_.push(std::move(std::make_pair(value, capacity)));
    cur_capacity_ += capacity;
    empty_cond_.notify_all();
  }

  void pop(T& value) {
    std::unique_lock<std::mutex> l(mu_);
    empty_cond_.wait(l, [this]{ return !queue_.empty(); });
    std::pair<T, size_t> e = std::move(queue_.front());
    value = std::move(e.first);
    cur_capacity_ -= e.second;
    queue_.pop();
    full_cond_.notify_all();
  }

  size_t size() const {
    std::lock_guard<std::mutex> l(mu_);
    return queue_.size();
  }

  bool empty() const {
    return size() == 0;
  }

 private:
  mutable std::mutex mu_;
  size_t max_capacity_ = 0, cur_capacity_ = 0;
  std::queue<std::pair<T, size_t> > queue_;
  std::condition_variable empty_cond_, full_cond_;
};
} // namespace PS
