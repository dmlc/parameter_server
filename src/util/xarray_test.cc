#include "gtest/gtest.h"
#include "util/xarray.h"

using namespace PS;

TEST(XArray, LowerBound) {
  XArray<int> a{1,3,4,5};

  XArray<int> x = {1,2,4,5,6};
  XArray<int> y = {1,1,4,5,5};

  EXPECT_TRUE(a.LowerBound(0)==NULL);

  for (int i = 0; i < 5; ++i)
    EXPECT_EQ(*a.LowerBound(x[i]), y[i]);
}

TEST(XArray, UpperBound) {
  XArray<int> a = {1,3,4,5};
  XArray<int> x = {0,1,2,4,5};
  XArray<int> y =  {1,1,3,4,5};

  for (int i = 0; i < 5; ++i)
    EXPECT_EQ(*a.UpperBound(x[i]), y[i]);
  EXPECT_TRUE(a.UpperBound(6)==NULL);
}

TEST(XArray, Pointers) {
  XArray<int> a = {1,3,4,5};

  auto x = a.UpperBound(5);
  auto y = a.LowerBound(2);
  EXPECT_EQ((int)(x-y), 3);
}
