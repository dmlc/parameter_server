#pragma once
#include "util/common.h"
#include "data/common.h"
#include "util/recordio.h"
#include "proto/example.pb.h"
namespace PS {

DEFINE_int32(n, 3, "show the first *n* instances in text format");

static void showExample() {
  File* in = File::openOrDie(FLAGS_input, "r");
  RecordReader reader(in);
  for (int i = 0; i < FLAGS_n; ++i) {
    Example ex;
    CHECK(reader.ReadProtocolMessage(&ex));
    std::cout << ex.ShortDebugString() << std::endl;
  }
}

} // namespace PS
