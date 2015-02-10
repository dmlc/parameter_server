#include "gtest/gtest.h"
#include "util/shared_array_inl.h"

using namespace PS;

TEST(SArray, compress) {
  int n = 100000;
  SArray<double> a(n), aa(n);
  SArray<float> b(n), bb(n);

  for (int i = 0; i < n; ++i) {
    float x = (float)rand() / (float) RAND_MAX;
    a[i] = x;
    b[i] = x;
  }

  LL << a.size() << " " << b.size();
  LL << a.eigenVector().norm() << " " << b.eigenVector().norm();

  // LL << a;
  auto ca = a.compressTo();
  auto cb = b.compressTo();
  LL << ca.size() << " " << cb.size();

  aa.uncompressFrom(ca);
  bb.uncompressFrom(cb);

  // LL << aa;
  // LL << bb;

  LL << aa.eigenVector().norm() << " " << bb.eigenVector().norm();
}
