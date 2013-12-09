#include "gtest/gtest.h"
#include "box/consistency.h"

using namespace PS;
// TODO didn't test PUSH & REPLY

int itv = 2; // 10 millisecons

void DoClear(int *order, int n, Consistency<bool>* con) {
  Header flag;
  for (int i = 0; i < n; ++i) {
    std::this_thread::sleep_for(milliseconds(itv));
    flag.set_time(order[i]);
    flag.set_type(Header::PUSH);
    con->ResponsePush(flag, i>3);
  }
}

TEST(Consistency, ClearPush) {
  int n = 5;
  int recv_order[] = {1,5,2,3,6};
  int expect_order[] = {1,2,3,5,6,8};
  int expect_time[]  = {0,1,3,4,4,5};
  for (int rp = 0; rp < 50; rp++) {
    Consistency<bool> con;
    con.SetMaxDelay(1,0);
    std::thread p(DoClear, recv_order, n, &con);
    Header flag;
    auto start = tic();
    for (int i = 0; i <= n; i++) {
      flag.set_time(expect_order[i]);
      flag.set_type(Header::PUSH);
      con.Request(&flag, NULL, NULL);
      auto t = toc(start);
      EXPECT_LT(t, expect_time[i]*itv+1);
      EXPECT_GT(t, expect_time[i]*itv-1);
    }
    p.join();
  }
}
