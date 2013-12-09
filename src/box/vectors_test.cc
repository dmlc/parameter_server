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
  w->Vec(1) = w->Vec(0);
}

TEST(Vectors, Aggregator) {

  int n = 6;  // global #feature
  int m = 4;  // local #feature
  // local gradients
  DVec g1 = DVec::Ones(4);
  DVec g2 = DVec::Ones(4);

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
  // srand(time(0)+my_seed);

  int delay = 1;
  double actual_delay;

  if (FLAGS_my_type == "c") {
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
    // first col is weight, second is gradient
    Vectors<double> w("haha", n, 2, keys);
    w.SetMaxDelay(10000,delay);
    w.Vec(0) = DVec::Zero(4);

    for (int i = 1; i < 100; ++i) {
      w.Vec(0) = g * i * i;
      w.PushPull(KeyRange::All(), {0}, kValue, {1}, kDelta);
      if (FLAGS_my_rank ==0)
        LL << w.Vec(1).sum() / res;
        // LL << w.vecs().col(1).sum() / res;
    }
    std::this_thread::sleep_for(seconds(5));
  } else {
    Vectors<double> w("haha", n, 2);
    w.SetAggregator(NodeGroup::kClients);
    w.SetAggregatorFunc(NewPermanentCallback(MoveData, &w));
    std::this_thread::sleep_for(seconds(30));
  }

  int ret; wait(&ret);
}
