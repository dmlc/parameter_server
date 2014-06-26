#include "gtest/gtest.h"
#include "base/bitmap.h"
using namespace PS;

TEST(Bitmap, SetClear) {
  Bitmap x(100, true);
  x.clear(30);
  EXPECT_EQ(x.test(30), false);

}
TEST(Bitmap, NNZ) {
  Bitmap x(100, true);
  EXPECT_EQ(x.nnz(), 100);

  for (int i = 0; i < 10; i++) {
    std::vector<int> idx(100);
    for (int j = 0; j < 100; ++j) idx[j] = j;
    std::random_shuffle(idx.begin(), idx.end());
    for (int j = 0; j < 30; ++j) x.clear(idx[j]);
    EXPECT_EQ(x.nnz(), 70);
  }
}
