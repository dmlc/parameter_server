#include "gtest/gtest.h"
#include "data/group_reader.h"

using namespace PS;

TEST(GroupReader, read) {
  DataConfig cache, dc;
  cache.add_file("/tmp/test/");
  dc.set_format(DataConfig::TEXT);
  dc.set_text(DataConfig::ADFEA);
  dc.add_file("../../data/ctrc/train/part-000[0-1].gz");

  GroupReader gr(cache);
  gr.read(searchFiles(dc));
}
