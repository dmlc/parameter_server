#include "gtest/gtest.h"
#include "util/hashfunc.h"

using namespace PS;

TEST(HashFunc, HashToKeyRange) {
  HashFunc hashFunc;

  Key min_key = 1;
  Key max_key = 1000;
  
  Key hashed_1 = hashFunc.HashToKeyRange(min_key, max_key);
  ASSERT_LE(hashed_1, max_key);
  ASSERT_GE(hashed_1, min_key);
  
  Key hashed_2 = hashFunc.HashToKeyRange(min_key, max_key);
  ASSERT_LE(hashed_2, max_key);
  ASSERT_GE(hashed_2, min_key);

  Key hashed_3 = hashFunc.HashToKeyRange(min_key, max_key);
  ASSERT_LE(hashed_3, max_key);
  ASSERT_GE(hashed_3, min_key);

  Key hashed_4 = hashFunc.HashToKeyRange(min_key, max_key);
  ASSERT_LE(hashed_4, max_key);
  ASSERT_GE(hashed_4, min_key);
  
  EXPECT_NE(hashed_1, hashed_2);
  EXPECT_NE(hashed_1, hashed_3);
  EXPECT_NE(hashed_1, hashed_4);
  EXPECT_NE(hashed_2, hashed_3);
  EXPECT_NE(hashed_2, hashed_4);
  EXPECT_NE(hashed_3, hashed_4);
}
