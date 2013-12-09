#include "gtest/gtest.h"
#include "util/key.h"

using namespace PS;
TEST(Key, LowerBound) {
  int a[] = {1,3,4,5};

  int x[] = {0,1,2,4,5,6};
  int y[] = {-1,0,0,2,3,3};

  for (int i = 0; i < 6; ++i)
    EXPECT_EQ(LowerBound<int>(a, 4, x[i]), y[i]);
}

TEST(Key, UpperBound) {
  int a[] = {1,3,4,5};

  int x[] = {0,1,2,4,5,6};
  int y[] = {0,0,1,2,3,4};

  for (int i = 0; i < 6; ++i)
    EXPECT_EQ(UpperBound<int>(a, 4, x[i]), y[i]);
}

void CheckArray(RawArray a, int* b) {
  Key *k = (Key *) a.data();
  EXPECT_TRUE(a.entry_num() > 0);
  for (int i = 0; i < a.entry_num(); ++i) {
    EXPECT_EQ(k[i], b[i]);
  }
}

void CheckDArray(RawArray a, int* b, int n) {
  double *k = (double *) a.data();
  EXPECT_EQ(a.entry_num(), n);
  for (int i = 0; i < a.entry_num(); ++i) {
    EXPECT_EQ(k[i], (double)b[i]);
  }
}

TEST(Key, Slice) {

  Key *k = new Key[6];
  k[0] = 1;
  k[1] = 3;
  k[2] = 4;
  k[3] = 10;
  k[4] = 100 ;
  k[5] = 1000;
  RawArray a((char*)k, sizeof(Key), 6);

  double *d = new double[6];
  d[0] = 4;
  d[1] = 1;
  d[2] = 78;
  d[3] = 34;
  d[4] = 2;
  d[5] = 1;
  RawArray b((char*)d, sizeof(double), 6);
  int d1[] = {4,1,78};
  int d2[] = {78,34};
  int d3[] = {4,1,78,34,2,1};

  KeyRange r1(0,5);
  int v1[] = {1,3,4};

  KeyRange r2(4,100);
  int v2[] = {4,10};

  KeyRange r3(0,30000);
  int v3[] = {1,3,4,10,100,1000};

  RawArray c1 = Slice(a, r1);
  RawArray c2 = Slice(a, r2);
  RawArray c3 = Slice(a, r3);
  CheckArray(c1, v1);
  CheckArray(c2, v2);
  CheckArray(c3, v3);

  CheckDArray(Slice(a, b, c1), d1, 3);
  CheckDArray(Slice(a, b, c2), d2, 2);
  CheckDArray(Slice(a, b, c3), d3, 6);

  Key *k1 = new Key[1];
  Key *k2 = new Key[2];
  Key *k3 = new Key[3];
  Key *k4 = new Key[4];
  k1[0] = 10;
  k2[0] = 2;
  k2[1] = 11;
  k3[0] = 1;
  k3[1] = 2;
  k3[2] = 100;
  k4[0] = 0;
  k4[1] = 1;
  k4[2] = 10;
  k4[3] = 9999;
  int w1[] = {34};
  int w2[] = {};
  int w3[] = {4,2};
  int w4[] = {4,34};
  RawArray a1((char*)k1, sizeof(Key), 1);
  RawArray a2((char*)k2, sizeof(Key), 2);
  RawArray a3((char*)k3, sizeof(Key), 3);
  RawArray a4((char*)k4, sizeof(Key), 4);

  CheckDArray(Slice(a, b, a1), w1, 1);
  CheckDArray(Slice(a, b, a2), w2, 0);
  CheckDArray(Slice(a, b, a3), w3, 2);
  CheckDArray(Slice(a, b, a4), w4, 2);
}
