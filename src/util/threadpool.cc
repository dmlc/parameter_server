#include "util/threadpool.h"

namespace PS {

ThreadPool::~ThreadPool() {
  if (!started_) return;
  // if (started_) {
    mu_.lock();
    waiting_to_finish_ = true;
    cv_.notify_all();
    mu_.unlock();

    StopOnFinalBarrier();

    for (int i = 0; i < num_workers_; ++i)
      all_workers_[i].join();
  // }
}

void RunWorker(void* data) {
  ThreadPool* const thread_pool = reinterpret_cast<ThreadPool*>(data);
  auto task = thread_pool->GetNextTask();
  while (task) {
    task();
    task = thread_pool->GetNextTask();
  }
  thread_pool->StopOnFinalBarrier();
}

void ThreadPool::StartWorkers() {
  started_ = true;
  for (int i = 0; i < num_workers_; ++i)
    all_workers_.push_back(std::move(std::thread(&RunWorker, this)));
}

typename ThreadPool::Task ThreadPool::GetNextTask() {
  std::unique_lock<std::mutex> l(mu_);
  for (;;) {
    if (!tasks_.empty()) {
      auto task = tasks_.front();
      tasks_.pop_front();
      return task;
    }

    if (waiting_to_finish_)
      return Task();
    else
      cv_.wait(l);
  }
  return Task();
}

void ThreadPool::Add(const Task& task) {
  std::lock_guard<std::mutex> l(mu_);
  tasks_.push_back(task);
  if (started_) cv_.notify_all();
}

}  // namespace PS
