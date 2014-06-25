#include "gtest/gtest.h"
#include "base/io.h"
using namespace PS;

#include <regex>
// TEST(IO, ReadFilenames) {
//   auto files = readFilenamesInDirectory("../data/recordio/");
//   for (auto f : files) LL << f;
// }

TEST(IO, xxx) {
  try {

 std::regex xx("ctr4m_part_.*");
  }

  catch (const std::regex_error& e) {
    std::cout << "regex_error caught: " << e.what() << '\n';
    if (e.code() == std::regex_constants::error_brack) {
      std::cout << "The code was error_brack\n";
    }
  }
}

TEST(IO, matchFilenames) {

  DataConfig cf;
  cf.add_files("../data/recordio/rcv1_.*");
  searchFiles(cf);
}
