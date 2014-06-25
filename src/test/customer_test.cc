#include "gtest/gtest.h"
#include "system/customer.h"
#include "proto/config.pb.h"

#include "google/protobuf/text_format.h"
using namespace PS;
TEST(ANY, PROTO) {

  GradDescConfig c;

  LL << c.ShortDebugString();
  string s;
  google::protobuf::TextFormat::PrintToString(c, &s);
  // c.SerializeToString(&s);
  GradDescConfig d;
  google::protobuf::TextFormat::ParseFromString(s, &d);
  // d.ParseFromString(s);
  LL << d.ShortDebugString();
  LL << d.app_name();
  LL << d.weight_name();
}
// TEST(Customer, GetID) {
//   // FORK1C1S;

//   for (int i = 0; i < 10; ++i) {
//     // Customer o("o" + to_string(i));
//     // EXPECT_EQ(o.id(), i);
//   }

//   for (int i = 0; i < 10; ++i) {
//     // Customer o("o" + to_string(i));
//     // EXPECT_EQ(o.id(), i);
//   }
//   usleep(100);
//   // WAIT;
// }

// int main(int argc, char **argv) {
//   testing::InitGoogleTest(&argc, argv);
//   google::ParseCommandLineFlags(&argc, &argv, true);
//   FLAGS_logtostderr = 1;

//   return RUN_ALL_TESTS();
// }
