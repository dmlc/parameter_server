#include "gtest/gtest.h"
#include <float.h>
#include "base/shared_array.h"


using namespace PS;

TEST(double, comression) {
  const int n = 10000;
  SArray<double> x(n);
  SArray<double> y(n);

  // uint64 ff = 0xf000000000000000;
  uint64 ff = 0xffffffffffffffff;
  std::vector<int> idx(n);
  for (int i = 0; i < n; ++i) idx[i] = i;

  for (int t = 0; t < 5; ++t) {
    std::random_shuffle(idx.begin(), idx.end());
    int k = 2 * n / 10;
    for (int j = 0; j < n; ++j) {
      int p  = idx[j];
      if (j < k) {
        double v = random() / 1e5;
        x[p] = v;
        y[p] = v;
      } else {
        x[p] = 0;
        // y[p] = 65525;
        // y[p] =  std::numeric_limits<double>::infinity();
        y[p] = *((double*)&ff);
      }
    }
    LL << x.compressTo().size();
    LL << y.compressTo().size();
  }

}

TEST(double, max) {
  uint64 x = 0xffffffffffffffff;
  LL << x << " " <<  (double)x;
  double y = *((double*)&kuint64max);


  LL << (y != y);
}
