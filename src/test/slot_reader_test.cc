#include "gtest/gtest.h"
#include "data/slot_reader.h"

using namespace PS;

TEST(SlotReader, read) {
  DataConfig cache, dc;
  cache.add_file("/tmp/test/");
  dc.set_format(DataConfig::TEXT);

  // load adfea
  dc.set_text(DataConfig::ADFEA);
  dc.add_file("../../data/ctrc/train/part-000[0-1].gz");

  // load libsvm
  // dc.set_text(DataConfig::LIBSVM);
  // dc.add_file("../data/rcv1/train/part-.*");

  DataConfig dc2 = searchFiles(dc);
  SlotReader gr; gr.init(dc2, cache); gr.read();

  auto data = readMatricesOrDie<double>(dc2);

  auto label = gr.value<double>(0);
  // LL << label;
  // LL << data[0]->value();
  EXPECT_EQ((label.eigenVector() - data[0]->value().eigenVector()).norm(), 0);

  for (int i = 1; i < data.size(); ++i) {
    auto X = std::static_pointer_cast<SparseMatrix<uint64, double>>(data[i]);
    int id = X->info().id();
    auto index  = gr.index(id);
    auto offset = gr.offset(id);
    // LL << index;
    // LL << offset;
    EXPECT_EQ((index.eigenVector() - X->index().eigenVector()).norm(), 0);
    EXPECT_EQ((offset.eigenVector() - X->offset().eigenVector()).norm(), 0);

    if (!X->value().empty()) {
      auto value = gr.value<double>(id);
      EXPECT_EQ((value.eigenVector() - X->value().eigenVector()).norm(), 0);
    }
  }
}
