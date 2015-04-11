#include "gtest/gtest.h"
#include "util/common.h"
#include "util/resource_usage.h"
#include "util/threadsafe_queue.h"
using namespace PS;

std::shared_ptr<int> p(new int());
inline void fun1(std::shared_ptr<int> a) {
  *a += 1;
}

inline void fun2(std::shared_ptr<int>& a) {
  *a += 1;
}

void run(int t) {
  int n = 100000;
  if (t == 1){
    for (int i = 0; i < n; ++i) fun1(p);
  } else {
    for (int i = 0; i < n; ++i) fun2(p);
  }
}

TEST(xx, xx) {
  *p = 1;
  auto tv = tic();
  std::thread p1(run, 1);
  std::thread p2(run, 1);
  p1.join();
  p2.join();
  LL << toc(tv) << " " << *p;

  *p = 1;
  tv = tic();
  std::thread p3(run, 2);
  std::thread p4(run, 2);
  p3.join();
  p4.join();
  LL << toc(tv) << " " << *p;

}


TEST(xx,bb) {
  ThreadsafeQueue<std::unique_ptr<int>> queue;

  std::unique_ptr<int> a(new int());
  *a = 1;
  LL << a.get();
  queue.push(std::move(a));
  LL << a.get();

  std::unique_ptr<int> b;
  queue.wait_and_pop(b);
  LL << b.get();
}
