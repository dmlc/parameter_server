#include "gtest/gtest.h"
#include "base/shared_array_inl.h"
#include "util/resource_usage.h"

using namespace PS;

TEST(SArray, Ctor) {
  std::vector<int> a = {1, 4, 3, 2};
  SArray<int> sa; sa.copyFrom(a.begin(), a.end());
  {
    SArray<uint32> sb(sa);
    EXPECT_EQ(sa.pointer().use_count(), 2);
  }
  EXPECT_EQ(sa.pointer().use_count(), 1);
}

// TEST(SArray, Mem) {
//   ResUsage r;
//   {
//     std::vector<int> a(10000000);
//     double mem = 2*r.myPhyMem() + 1;
//     {
//       // SharedArray<int> sa(a.begin(), a.end());
//       SArray<int> sa; sa.copyFrom(a.begin(), a.end());
//       EXPECT_LE(r.myPhyMem(), mem);
//       {
//         SArray<int> sb(sa);
//         EXPECT_LE(r.myPhyMem(), mem);
//         {
//           SArray<char> sc(sa);
//           EXPECT_LE(r.myPhyMem(), mem);
//         }
//         EXPECT_LE(r.myPhyMem(), mem);
//       }
//       EXPECT_LE(r.myPhyMem(), mem);
//     }
//     EXPECT_LE(r.myPhyMem(), mem);
//   }
// }

TEST(SArray, intersection) {
  SArray<int> a{1,2,3,5,6,7,8}, b{3,4,7,10}, c{3,7}, d{};
  EXPECT_EQ(a.setIntersection(b), c);
  EXPECT_EQ(b.setIntersection(a), c);

  EXPECT_EQ(a.setIntersection(d), d);
  EXPECT_EQ(d.setIntersection(a), d);
}

TEST(SArray, union) {
  SArray<int> a{3,5,8,10}, b{5,9,10,11}, c{3,5,8,9,10,11}, d{};
  EXPECT_EQ(a.setUnion(b), c);
  EXPECT_EQ(b.setUnion(a), c);

  EXPECT_EQ(a.setUnion(d), a);
  EXPECT_EQ(d.setUnion(a), a);
}

TEST(SArray, range) {
  SArray<int> a{1,1,4,4,5,7,7,8,8,8,20}, b{1,1}, c{4,4,5,7,7,8,8,8}, d{};

  // EXPECT_EQ(a.findRange(1, 4), b);
  // EXPECT_EQ(a.findRange(2, 15), c);
  // EXPECT_EQ(a.findRange(0, 100), a);
  // EXPECT_EQ(a.findRange(100, 0), d);
  // EXPECT_EQ(a.findRange(4, 4), d);
}

TEST(SArray, gz) {
  SArray<int> a; a.readFromFile("../data/bin/rcv1_X.index", 1498952);
  SArray<int> b; b.readFromFile("../data/bin_gz/rcv1_X.index.gz", 1498952);
  // LL << a.eigenVector().sum() << " " << b.eigenVector().sum();
  EXPECT_EQ(a,b);

  SArray<double> x; x.readFromFile("../data/bin/rcv1_X.value", 1498952);
  SArray<double> y; y.readFromFile("../data/bin_gz/rcv1_X.value.gz", 1498952);
  // LL << x.eigenVector().sum() << " " << y.eigenVector().sum();
  EXPECT_EQ(x,y);
}
