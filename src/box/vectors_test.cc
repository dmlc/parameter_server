#include "gtest/gtest.h"
#include "box/vectors.h"

using namespace PS;

TEST(Vectors, xxx) {
  map<int, Header> hs;

  {
  Header x;
  {
    Header h;
    h.mutable_key()->set_empty(1);
    h.mutable_value()->set_empty(1);
    h.mutable_push()->set_delta(1);
    h.mutable_pull()->set_delta(1);
    hs[0] = h;
    x = h;
  }
  }
  hs.erase(0);

}

void MoveData(Vectors<double> *w) {
  w->vec(1) = w->vec(0);
  w->vec(0) *= 0;
  LL << w->vec(0).sum() << " " << w->vec(1).sum();
}

TEST(Vectors, Aggregator) {

  int n = 6;  // global #feature
  int m = 4;  // local #feature
  // local gradients
  DVec g1 = DVec::Ones(4);
  DVec g2 = DVec::Ones(4);

  // FLAGS_num_client = 2;
  // FLAGS_num_server = 2;
  // FLAGS_my_type = "s";
  // pid_t pid = fork();
  // if (pid == 0) {
  //   FLAGS_my_type = "c";
  //   pid_t pid2 = fork();
  //   if (pid2 == 0) {
  //     FLAGS_my_type = "c";
  //     FLAGS_my_rank ++;
  //     pid_t pid3 = fork();
  //     if (pid3 == 0)
  //       FLAGS_my_type = "s";
  //   }
  // }
  // // srand(time(0)+my_seed);

  int delay = 0;
  double actual_delay;

  // s0: v v v
  // s1:       v v v
  // c0: v v   v v
  // c1:   v v   v v

  if (FLAGS_my_type == "client") {
    // local keys
    XArray<Key> keys;
    DVec g;
    double res;
    if (FLAGS_my_rank == 0) {
      keys = XArray<Key>({0,1,3,4});
      g = g1;
      DVec r = g; r[1] += g2[0]; r[3] += g2[2];
      res = r.sum();
    } else {
      keys = XArray<Key>({1,2,4,5});
      g = g2;
      DVec r = g; r[0] += g1[1]; r[2] += g1[3];
      res = r.sum();
    }
    // the first col is weight, the second is gradient
    Vectors<double> w("haha", n, {kWrite, kRead}, keys);
    w.SetMaxDelay(10000,delay);
    w.vec(0) = DVec::Zero(4);

    for (int i = 1; i < 3; ++i) {
      w.vec(0) = g * i;
      w.PushPull(KeyRange::All(), {0}, kValue, {1}, kDelta);
      // if (FLAGS_my_rank ==0) {
      LL <<  w.vec(1).sum(); //  / res ;
      // }
    }
    std::this_thread::sleep_for(seconds(2));
  } else {
    Vectors<double> w("haha", n, {kRead, kWrite});
    w.SetAggregator(NodeGroup::kClients);
    w.SetAggregatorFunc(NewPermanentCallback(MoveData, &w));
    std::this_thread::sleep_for(seconds(10));
  }

  int ret; wait(&ret);
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  google::ParseCommandLineFlags(&argc, &argv, true);
  RUN_ALL_TESTS();
  // LL << "exists";
}
