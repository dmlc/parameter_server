#include "gtest/gtest.h"
#include "gflags/gflags.h"
#include "glog/logging.h"

int main(int argc, char **argv) {
  FLAGS_logtostderr = 1;
  testing::InitGoogleTest(&argc, argv);
  google::ParseCommandLineFlags(&argc, &argv, true);

  return RUN_ALL_TESTS();
}
