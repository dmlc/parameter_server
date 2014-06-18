#include "gtest/gtest.h"
#include "base/io.h"
using namespace PS;

// TEST(IO, ReadFilenames) {
//   auto files = readFilenamesInDirectory("../data/recordio/");
//   for (auto f : files) LL << f;
// }


TEST(IO, matchFilenames) {
  DataConfig cf;
  cf.add_files("../data/recordio/ctr4m_part_[0-4]");
  searchFiles(cf);
}
