#include "gtest/gtest.h"

using namespace PS;

class MatrixIOTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
  }

  void verifyRCV1(MatrixPtrList<double> data) {
    EXPECT_EQ(data.size(), 2);
    auto y = data[0]->value().eigenArray();
    EXPECT_EQ(y.size(), 20242);
    EXPECT_EQ(y.sum(), 740);

    auto X = std::static_pointer_cast<SparseMatrix<uint64, double> >(data[1]);
    auto os = X->offset().eigenArray();
    // auto idx = X->index().eigenArray();
    auto val = X->value().eigenArray();

    EXPECT_EQ(os.sum(), 15016151914);
    // EXPECT_EQ(idx.sum(), 35335196536);
    EXPECT_GE(val.sum(), 138760);
    EXPECT_LE(val.sum(), 138770);
    EXPECT_EQ(SizeR(1, 47237), SizeR(X->info().col()));

    // LL << data[0]->info().DebugString();
    // LL << data[0]->debugString();
    // LL << data[1]->debugString();
  }
};


TEST_F(MatrixIOTest, ReadRCV1Single) {
  DataConfig dc;
  dc.set_format(DataConfig::TEXT);
  dc.set_text(DataConfig::LIBSVM);
  dc.add_file("../data/rcv1_train.binary");

  auto data = readMatricesOrDie<double>(dc);
  verifyRCV1(data);
}

TEST_F(MatrixIOTest, ReadRCV1Multi) {
  DataConfig dc;
  dc.set_format(DataConfig::TEXT);
  dc.set_text(DataConfig::LIBSVM);
  dc.add_file("../data/rcv1/train/part.*");
  dc.add_file("../data/rcv1/test/part.*");
  auto data = readMatricesOrDie<double>(searchFiles(dc));
  verifyRCV1(data);
}

TEST_F(MatrixIOTest, ReadADFEA) {
  DataConfig dc;
  dc.set_format(DataConfig::TEXT);
  dc.set_text(DataConfig::ADFEA);
  dc.add_file("../../data/ctrc/train/part-0001.gz");
  auto data = readMatricesOrDie<double>(dc);

  // for (int i = 0; i < data.size(); ++i) {
  //   data[i]->info().clear_ins_info();
  //   LL << data[i]->debugString();
  // }
}
