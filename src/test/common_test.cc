#include "gtest/gtest.h"
#include "util/common.h"

using namespace PS;
TEST(Common, NumberOfSetBits) {
  EXPECT_EQ(NumberOfSetBits(0), 0);
  EXPECT_EQ(NumberOfSetBits(1), 1);
  EXPECT_EQ(NumberOfSetBits(2), 1);
  EXPECT_EQ(NumberOfSetBits(3), 2);
  EXPECT_EQ(NumberOfSetBits(4), 1);
  EXPECT_EQ(NumberOfSetBits(5), 2);
  EXPECT_EQ(NumberOfSetBits(6), 2);
  EXPECT_EQ(NumberOfSetBits(8), 1);
  EXPECT_EQ(NumberOfSetBits(9), 2);
  EXPECT_EQ(NumberOfSetBits(16), 1);
  EXPECT_EQ(NumberOfSetBits(kint32min), 1);
  EXPECT_EQ(NumberOfSetBits(kint32max), 31);
}

// TEST(COMMON, STRFY) {
//   ASSERT_EQ("", strfy(""));

//   ASSERT_EQ("0", strfy(0));
//   ASSERT_EQ("123.4", strfy(123.4));
//   ASSERT_EQ("tcp://localhost:7000", strfy("tcp://localhost:7000"));

//   char cstr[] = "tcp://localhost:8000";
//   ASSERT_EQ("tcp://localhost:8000", strfy(cstr));
// }

// TEST(COMMON, SPLIT) {
//   string str1 =  "one:two:three";
//   std::vector<string> str1_sp = split(str1, ':');
//   ASSERT_EQ(3, str1_sp.size());
//   ASSERT_EQ("one", str1_sp[0]);
//   ASSERT_EQ("two", str1_sp[1]);
//   ASSERT_EQ("three", str1_sp[2]);

//   string str2 = "one#two##three";
//   std::vector<string> str2_sp = split(str2, '#');
//   ASSERT_EQ(4, str2_sp.size());
//   ASSERT_EQ("one", str2_sp[0]);
//   ASSERT_EQ("two", str2_sp[1]);
//   ASSERT_EQ("", str2_sp[2]);
//   ASSERT_EQ("three", str2_sp[3]);

//   string str3 = "one#two#three##";
//   std::vector<string> str3_sp = split(str3, '#');
//   ASSERT_EQ(4, str3_sp.size());
//   ASSERT_EQ("one", str3_sp[0]);
//   ASSERT_EQ("two", str3_sp[1]);
//   ASSERT_EQ("three", str3_sp[2]);
//   ASSERT_EQ("", str3_sp[3]);
// }
