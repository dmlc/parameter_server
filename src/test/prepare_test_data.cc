#include "data/text_parser.h"
#include "gtest/gtest.h"
#include "base/shared_array_inl.h"
using namespace PS;

SArray<Key> readKeyFromFile(string filename) {
  ExampleParser parser;
  parser.init(DataConfig::ADFEA, true);
  auto file = File::openOrDie(filename, "r");
  const int bs = 100000;
  char* buf = new char[bs];
  Example ex;
  SArray<Key> key;
  while (true) {
    char* line = file->readLine(buf, bs);
    if (line == NULL) break;
    parser.toProto(line, &ex);
    const auto& slot = ex.slot(1);
    for (int j = 0; j < slot.key_size(); ++j) {
      key.pushBack(slot.key(j));
    }
  }
  return key;
}

TEST(PREPARE_DATA, KEY) {
  auto key1 = readKeyFromFile("../../data/ctrd/part-0000");
  CHECK(key1.writeToFile("../data/test/key.1"));

  auto key2 = readKeyFromFile("../../data/ctrd/part-0001");

  std::sort(key1.begin(), key1.end());
  std::sort(key2.begin(), key2.end());

  auto it2 = std::unique(key2.begin(), key2.end());
  key2.resize(it2 - key2.begin());
  CHECK(key2.writeToFile("../data/test/key.2"));

  SArray<Key> key3(key2.size());
  auto it3 = std::set_difference(
      key2.begin(), key2.end(), key1.begin(), key2.begin(), key3.begin());
  key3.resize(it3 - key3.begin());
  CHECK(key3.writeToFile("../data/test/key.3"));
}
