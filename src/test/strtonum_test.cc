#include "gtest/gtest.h"
#include "util/strtonum.h"
#include "util/common.h"

using namespace PS;

TEST(strtonum, float) {
  string a = "12c3.4", b = "a123.5", c = "343";
  float fa, fb, fc;
  EXPECT_EQ(strtofloat(a, &fa), 0);
  EXPECT_EQ(strtofloat(b, &fb), 0);
  EXPECT_EQ(strtofloat(c, &fc), 1);
  EXPECT_EQ(fc, 343);
  // EXPECT_EQ(std::to_string(fc), "343.23");
}

TEST(strtonum, int32) {
  string a = "12c3", b = "a123", c = "343";
  int32 fa, fb, fc;
  EXPECT_EQ(strtoi32(a, &fa), 0);
  EXPECT_EQ(strtoi32(b, &fb), 0);
  EXPECT_EQ(strtoi32(c, &fc), 1);
  EXPECT_EQ(fc, 343);
  // EXPECT_EQ(std::to_string(fc), "343.23");
}

TEST(strtonum, uint64) {
  string a = "12c3", b = "a123", c = "343";
  uint64 fa, fb, fc;
  EXPECT_EQ(strtou64(a, &fa), 0);
  EXPECT_EQ(strtou64(b, &fb), 0);
  EXPECT_EQ(strtou64(c, &fc), 1);
  EXPECT_EQ(fc, 343);
  // EXPECT_EQ(std::to_string(fc), "343.23");
}
