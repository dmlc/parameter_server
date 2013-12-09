#include "gtest/gtest.h"
#include "system/workload.h"
#include "util/xarray.h"
using namespace PS;

TEST(Workload, Cache) {
  XArray<Key> k(20);
  for (int i = 0; i < 20; ++i) k[i] = i;

  KeyRange r1 = KeyRange(3, 10);
  KeyRange r2 = KeyRange(5, 20);
  KeyRange r3 = KeyRange(20, 30);
  RawArray k1 = Slice(k.raw(), r1);
  RawArray k2 = Slice(k.raw(), r2);
  RawArray k3 = Slice(k.raw(), r2);
  RawArray k4 = Slice(k.raw(), r3);

  XArray<Key> kk(k3);
  kk[0] = 100;

  cksum_t c1 = k1.ComputeCksum();
  cksum_t c2 = k2.ComputeCksum();
  cksum_t c3 = k3.ComputeCksum();
  cksum_t c4 = k4.ComputeCksum();

  Workload wl;
  EXPECT_EQ(wl.GetCache(r1, c1), false);
  wl.SetCache(r1, c1, k1);

  RawArray y1;
  EXPECT_EQ(wl.GetCache(r1, c1, &y1), true);
  EXPECT_TRUE(y1 == k1);

  wl.SetCache(r2, c2, k2);
  EXPECT_EQ(wl.GetCache(r2, c3), false);

  wl.SetCache(r3, c4, k4);
  EXPECT_EQ(wl.GetCache(r3, c4), true);
}
