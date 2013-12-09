#include "gtest/gtest.h"
#include "box/item.h"

using namespace PS;

struct Progress {
  Progress(double v = 0, int64 n = 0) : objv(v), nnz(n) { }

  Progress& operator+=(const Progress& rhs) {
    objv += rhs.objv;
    nnz += rhs.nnz;
    return *this;
  }
  Progress operator+(const Progress& rhs) {
    Progress res = *this;
    res += rhs;
    return res;
  }

  double objv;
  int64 nnz;
};


TEST(Item, ReduceAll) {
  int my_rank = 1;
  pid_t pid = fork();
  if (pid == 0) {
    FLAGS_my_type = "Client";
    my_rank ++;
    pid_t pid2 = fork();
    if (pid2 == 0) {
      my_rank ++;
      FLAGS_my_rank ++;
    }
  }

  FLAGS_num_client = 2;
  int delay = my_rank * 5;
  int max_delay = 30;

  Item<Progress> item("haha");
  for (int i = 1; i < 4; ++i) {
    std::this_thread::sleep_for(milliseconds(delay));
    auto s = tic();
    std::this_thread::sleep_for(milliseconds(delay));
    std::shared_future<Progress> fut;
    Progress p(my_rank*i+.5, my_rank*i);
    item.data() = p;
    item.AllReduce(&fut);
    auto v = fut.get();
    auto t = toc(s);
    // EXPECT_LE(t, max_delay+1);
    EXPECT_EQ(t, max_delay - delay);
    EXPECT_EQ(v.objv, 6*i+1.5);
    EXPECT_EQ(v.nnz, 6*i);
    // LL << v.objv << " " << v.nnz;
    // sleep(1);
  }



  int ret;
  wait(&ret);
}
