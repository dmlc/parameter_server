// some quick codes
#include "util/common.h"
TEST(str2num, xx) {
  string a = "12345";

}
int main(int argc, char *argv[]) {
  FLAGS_logtostderr = 1;
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
