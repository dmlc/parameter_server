#include "gtest/gtest.h"
#include "base/localizer.h"
#include "base/matrix_io_inl.h"

using namespace PS;

TEST(Localizer, RCV1) {
  DataConfig cache, dc;
  cache.add_file("/tmp/test/");

  dc.set_format(DataConfig::TEXT);
  dc.set_text(DataConfig::LIBSVM);
  dc.add_file("../data/rcv1_train.binary");

  SlotReader sr(dc, cache);
  ExampleInfo info;
  sr.read(&info);

  Localizer<uint64, double> lc;

  SArray<uint64> key;
  SArray<uint32> freq;
  lc.countUniqIndex(sr.index(1), &key, &freq);

  EXPECT_EQ(key.eigenArray().sum(), 1051859373);
  EXPECT_EQ(freq.eigenArray().sum(), 1498952);
  EXPECT_EQ(freq.eigenArray().square().sum(), 1924492682);
  EXPECT_EQ(key.size(), freq.size());
  EXPECT_EQ(key.size(), 44504);

  int filter = 2;
  SArray<uint64> f_key;
  for (int i = 0; i < key.size(); ++i) {
    if (freq[i] > filter) f_key.pushBack(key[i]);
  }
  EXPECT_EQ(f_key.size(), 19959);

  auto X = std::static_pointer_cast<SparseMatrix<uint32, double>>(
      lc.remapIndex( 1, f_key, &sr));

  EXPECT_EQ(X->offset().eigenArray().sum(), 14702421805);
  SArray<uint64> idx;
  for (auto k : X->index()) idx.pushBack(k);
  EXPECT_EQ(idx.size(), 1467683);
  EXPECT_EQ(idx.eigenArray().sum(), 14708054959 - idx.size());
  EXPECT_LT(X->value().eigenArray().sum(), 132224);
  EXPECT_GT(X->value().eigenArray().sum(), 132223);

  // LL << X->debugString();
}

// TEST(Localizer, ADFEA) {
//   DataConfig dc;
//   dc.set_format(DataConfig::TEXT);
//   dc.set_text(DataConfig::ADFEA);
//   dc.add_file("../../data/ctrc/train/part-000[0-1].gz");
//   auto data = readMatricesOrDie<double>(searchFiles(dc));

//   for (int i = 1; i < data.size(); ++i) {
//     Localizer<uint64, double> lc(data[i]);
//     SArray<uint64> key;
//     SArray<uint32> freq;
//     lc.countUniqIndex(&key, &freq);

//     int filter = 4;
//     SArray<uint64> f_key;
//     for (int i = 0; i < key.size(); ++i) {
//       if (freq[i] > filter) f_key.pushBack(key[i]);
//     }
//     LL << f_key.size();
//     auto X = std::static_pointer_cast<SparseMatrix<uint32, double>>(lc.remapIndex(f_key));
//     if (X) {
//       LL << X->index().eigenArray().maxCoeff();
//     }
//     // LL << X->debugString();
//   }
// }
