#include "gtest/gtest.h"
#include "util/parallel_ordered_match.h"
#include "util/shared_array_inl.h"

using namespace PS;
namespace PS {
DEFINE_int32(num_threads, 2, "");
DEFINE_int32(k, 1, "k");
}  // namespace PS


class PMatchTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    key1.ReadFromFile("../test/keys/key_1");
    key2.ReadFromFile("../test/keys/key_2");
  };

  SArray<uint64> key1, key2;
};

TEST_F(PMatchTest, simple) {

}

TEST_F(PMatchTest, match) {

  int k = FLAGS_k;
  SArray<double> val1(key1.size()*k, 1);
  SArray<double> val2;

  size_t n = ParallelOrderedMatch(key1, val1, key2, &val2, k);

  LL << n << " " << val2;
}
