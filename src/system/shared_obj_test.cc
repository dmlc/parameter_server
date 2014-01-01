#include "gtest/gtest.h"
#include "system/shared_obj.h"
#include "system/node.h"

using namespace PS;

TEST(SharedObj, GetID) {
  FORK2C2S;

  for (int i = 0; i < 10; ++i) {
    SharedObj o(StrCat("o", i));
    o.Init();
    EXPECT_EQ(o.id(), i);
  }

  for (int i = 0; i < 10; ++i) {
    SharedObj o(StrCat("o", i));
    o.Init();
    EXPECT_EQ(o.id(), i);
  }
  sleep(1);
  WAIT;
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  google::ParseCommandLineFlags(&argc, &argv, true);

  return RUN_ALL_TESTS();
}
