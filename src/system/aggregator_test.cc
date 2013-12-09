#include "gtest/gtest.h"
#include "system/aggregator.h"

using namespace PS;
TEST(Aggregator, Success) {
  NodeGroup group;

  group.all()->push_back(0);
  group.all()->push_back(3);
  group.all()->push_back(4);
  group.all()->push_back(6);

  group.clients()->push_back(0);
  group.clients()->push_back(3);

  int t[] = {0,0,1,0,1,0,1,1,1,1};
  int id[] = {0,4,6,3,5,6,4,3,0,2};
  int r1[] = {0,0,0,1,0,1,0,0,1,1};
  int r2[] = {0,0,0,0,0,1,0,0,1,1};

  Aggregator agg1(NodeGroup::kClients);
  Aggregator agg2(NodeGroup::kAll);
  Mail m;
  for (int i = 0; i < 10; ++i) {
    m.flag().set_time(t[i]);
    m.flag().set_sender(id[i]);
    agg1.Insert(m);
    EXPECT_EQ(agg1.Success(t[i], group), r1[i]);
    agg2.Insert(m);
    EXPECT_EQ(agg2.Success(t[i], group), r2[i]);
  }
}
