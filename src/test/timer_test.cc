#include <unistd.h>
#include "gtest/gtest.h"
#include "util/timer.h"

using namespace PS;

#define MTIME 23456
double dtime = (double) (MTIME * 1e-6);

TEST(TIMER, REAL) {
  auto tv = tic();
  usleep(MTIME);
  double t = toc(tv);
  EXPECT_LE(t, dtime+1e-4);
  EXPECT_GE(t, dtime-1e-4);
}

TEST(TIMER, USER) {
  // auto tv = tic_user();
  // usleep(MTIME);
  // double t = toc_user(tv);
  // EXPECT_LE(t, 1e-4);
  // EXPECT_GE(t, 1e-4);
}

TEST(TIMER, HW) {
  auto tv = tic_hw();
  usleep(MTIME);
  double t = toc_hw(tv);
  EXPECT_LE(t, dtime+1e-4);
  EXPECT_GE(t, dtime-1e-4);
}
