#include "gtest/gtest.h"
#include "box/vectors-inl.h"

using namespace PS;

void MoveData(Vectors<double> *w) {
  w->vec(2) = w->vec(0) + w->vec(1);
  w->vec(3) = w->vec(2) * 2;
  w->reset_vec(0);
  w->reset_vec(1);
  // LL << w->DebugString();
}

void RunClient(int delay, int max_iter, Vectors<double>* w) {
  size_t m = w->len();
  w->SetMaxPullDelay(delay);
  w->vec(0) = DVec::Zero(m);
  w->vec(1) = DVec::Zero(m);
  double factor = 18.0;
  for (int i = 0; i <= max_iter; ++i) {
    w->vec(0) = DVec::Ones(m) * (1<<(max_iter - i));
    w->vec(1) = w->vec(0)*2;
    w->PushPull(KeyRange::All(), {0,1}, kValue, {2,3}, kDelta);
    // LL << w.DebugString();
    double expect_min = (1<<(max_iter+1)) - (1<<(max_iter+1-std::max(0,i-delay)));
    double expect_max = (1<<(max_iter+1)) - (1<<(max_iter-i));
    // LL << i << " " << res/6.0 << " " << expect_min << " " << expect_max;
    double res2 = w->vec(2).sum() / factor;
    EXPECT_LE(res2, expect_max);
    EXPECT_GE(res2, expect_min);
    double res3 = w->vec(3).sum() / factor / 2;
    EXPECT_LE(res3, expect_max);
    EXPECT_GE(res3, expect_min);
  }
  w->Wait();
  EXPECT_EQ(w->vec(2).sum()/factor, (1<<(max_iter+1))-1);
  EXPECT_EQ(w->vec(3).sum()/factor/2, (1<<(max_iter+1))-1);
}

TEST(Vectors, Delays) {
  FLAGS_num_client = 2;
  FLAGS_num_server = 2;
  FLAGS_my_type = "s";
  pid_t pid = fork();
  if (pid == 0) {
    FLAGS_my_type = "c";
    pid_t pid2 = fork();
    if (pid2 == 0) {
      FLAGS_my_type = "c";
      FLAGS_my_rank ++;
      pid_t pid3 = fork();
      if (pid3 == 0)
        FLAGS_my_type = "s";
    }
  }

  // constructed vectors
  // s0: v v v
  // s1:       v v v
  // c0: v v   v v
  // c1:   v v   v v
  int n = 6;
  if (Node::Client(FLAGS_my_type)) {
    // local keys
    XArray<Key> keys;
    if (FLAGS_my_rank == 0) {
      keys = XArray<Key>({0,1,3,4});
    } else {
      keys = XArray<Key>({1,2,4,5});
    }
    // the first col is weight, the second is gradient
    int delay[] = {0,2,4,6};
    for (int i = 0; i < 4; ++i) {
      auto p = new Vectors<double>(StrCat("w",i), n, 4, keys);
      RunClient(delay[i], 20, p);
    }
  } else {
    for (int i = 0; i < 4; ++i) {
      auto p = new Vectors<double>(StrCat("w",i), n, 4);
      p->SetAggregator(NodeGroup::kClients);
      p->SetAggregatorFunc(NewPermanentCallback(MoveData, p));
    }
    std::this_thread::sleep_for(seconds(2));
  }

  int ret; wait(&ret);
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  google::ParseCommandLineFlags(&argc, &argv, true);

  return RUN_ALL_TESTS();
}
