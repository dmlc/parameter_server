#include "gtest/gtest.h"
#include "util/countmin.h"
using namespace PS;

TEST(xx, xx) {
  std::shared_ptr<int> p(new int());


}

// class CountMinTest : public ::testing::Test {
//  protected:
//   virtual void SetUp() {
//     int m = 1000;

//     key.resize(n);
//     cnt.resize(n);
//     for (int i = 0; i < n; ++i) {
//       uint64 a = (uint64) rand();
//       uint64 b = (uint64) rand();
//       key[i] = a | (b << 32);
//       cnt[i] = rand() % m;
//     }

//     auto tv = tic();
//     for (int i = 0; i < n; ++i) map[key[i]] += cnt[i];
//     LL << toc(tv);
//   }

//   void test(int len, int k) {
//     CountMin cm;
//     cm.resize(len, k);
//     auto tv = tic();
//     cm.bulkInsert(key, cnt);
//     auto t = toc(tv);

//     double err = 0, tol = 0;
//     for (int i = 0; i < n; ++i) {
//       int a = cm.query(key[i]);
//       int b = map[key[i]];
//       EXPECT_GE(a, b);
//       err += (a-b);
//       tol += b;
//     }
//     LL << t << " " << err / tol;
//   }
//   int n = 1000000;
//   SArray<uint64> key;
//   SArray<uint32> cnt;
//   std::unordered_map<uint64, uint32> map;
// };

// TEST_F(CountMinTest, test) {

//   for (int k = 1; k < 10; ++k) {
//     test(n*.2, k);
//     test(n, k);
//     test(n*2, k);
//     test(n*5, k);
//     test(n*10, k);
//   }
// }
