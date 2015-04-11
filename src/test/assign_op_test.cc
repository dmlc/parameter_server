#include "gtest/gtest.h"
#include "util/assign_op.h"
using namespace PS;
// evaluation the performance of assignop comparing to the plain version

size_t n = 1000000000;

TEST(AssignOp, OpPlus) {
  double a = 0;
  double b = 1;
  for (int i = 0; i < n; ++i) {
    AssignOp(a, b, AssignOpType::PLUS);
  }
  EXPECT_EQ(a,(double)n);
}

TEST(AssignOp, PlusPlain) {
  double a = 0;
  double b = 1;
  for (int i = 0; i < n; ++i) {
    a += b;
  }
  EXPECT_EQ(a,(double)n);
}

TEST(AssignOp, OpSet) {
  double a = 0;
  double b = 1;
  for (int i = 0; i < n; ++i) {
    AssignOp(a, b, AssignOpType::ASSIGN);
  }
  EXPECT_EQ(a,b);
}

TEST(AssignOp, SetPlain) {
  double a = 0;
  double b = 1;
  for (int i = 0; i < n; ++i) {
    a = b;
  }
  EXPECT_EQ(a,b);
}
