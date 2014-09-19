#include "gtest/gtest.h"
#include "base/matrix_io_inl.h"

using namespace PS;

class MatrixIOTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
  }
};


TEST_F(MatrixIOTest, ReadRCV1) {
  DataConfig dc;
  dc.set_format(DataConfig::TEXT);
  dc.set_text(DataConfig::LIBSVM);
  dc.add_file("../data/rcv1_train.binary");

  auto data = readMatricesOrDie<double>(dc);
  EXPECT_EQ(data.size(), 2);
  auto y = data[0]->value().eigenArray();
  EXPECT_EQ(y.size(), 20242);
  EXPECT_EQ(y.sum(), 740);

  auto X = std::static_pointer_cast<SparseMatrix<uint64, double> >(data[1]);
  auto os = X->offset().eigenArray();
  auto idx = X->index().eigenArray();
  auto val = X->value().eigenArray();

  EXPECT_EQ(os.sum(), 15016151914);
  EXPECT_EQ(idx.sum(), 35335196536);
  EXPECT_GE(val.sum(), 138760);
  EXPECT_LE(val.sum(), 138770);
  EXPECT_EQ(SizeR(1, 47237), SizeR(X->info().col()));

  // LL << data[0]->debugString();
  // LL << data[1]->debugString();
}
