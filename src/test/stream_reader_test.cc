#include "gtest/gtest.h"
#include "data/stream_reader.h"
#include "base/matrix_io_inl.h"

using namespace PS;

TEST(StreamReader, read) {
  DataConfig dc;
  // load adfea
  dc.set_format(DataConfig::TEXT);
  dc.set_text(DataConfig::ADFEA);
  dc.add_file("../../data/ctrc/train/part-000[0-1].gz");
  dc.set_ignore_feature_group(true);

  // load libsvm
  // dc.set_text(DataConfig::LIBSVM);
  // dc.add_file("../data/rcv1/train/part-.*");

  DataConfig dc2 = searchFiles(dc);
  StreamReader<double> reader; reader.init(dc2);

  MatrixPtrList<double> X;
  reader.readMatrices(100, &X);
}
