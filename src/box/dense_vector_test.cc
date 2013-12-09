#include "gtest/gtest.h"
#include "box/dense_vector.h"
using namespace PS;

TEST(DenseVector, Push) {
  int n = 100;
  dVec orig = dVec::Random(n);
  double norm = orig.norm();

  FLAGS_my_type = "server";
  int my_rank = 1;
  pid_t pid = fork();
  if (pid == 0) {
    FLAGS_my_type = "client";
    my_rank ++;
    pid_t pid2 = fork();
    if (pid2 == 0) {
      my_rank ++;
      FLAGS_my_rank ++;
    }
  }
  FLAGS_num_client = 2;
  srand(time(0)+my_rank);

  int delay = 3;
  double actual_delay;
  DenseVector<double> w("haha", n);
  if (FLAGS_my_type == "client") {
    w.SetMaxDelay(10000,delay);
    int n = 100;
    SyncFlag flag;
    flag.set_recver(NodeGroup::kServers);
    flag.set_type(SyncFlag::PUSH_PULL);
    flag.set_push_delta(true);
    flag.set_pull_delta(false);
    for (int i = 0; i < n; ++i) {
      w.vec() += orig;
      EXPECT_TRUE(w.Push(flag).ok());
      actual_delay =  FLAGS_num_client * i - w.vec().norm() / norm;

      // LL << w.SName() << actual_delay;
      std::this_thread::sleep_for(microseconds(500+rand()%100));
    }
    printf("act=%lf\n",actual_delay);
    // std::this_thread::sleep_for(seconds(1));
    for (int i = 0; i < 100; ++i) {
      EXPECT_TRUE(w.Push(flag).ok());
      actual_delay =  FLAGS_num_client * n - w.vec().norm() / norm;
      // LL << w.SName() << "fin" <<  actual_delay;
    }
    EXPECT_LT(fabs(actual_delay), 0.001);
  } else {
    std::this_thread::sleep_for(seconds(3));
  }



  int ret;
  wait(&ret);
}
