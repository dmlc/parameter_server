#include "gtest/gtest.h"

using namespace PS;

typedef SparseMatrix<int32, double> SM;
typedef Eigen::Matrix<double, Eigen::Dynamic, 1> Vec;

class SparseMatrixTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    // auto data =  //
    X = readMatrixFromBin<double>("../data/bin/rcv1_X");
  }

  MatrixPtr<double> y;
  MatrixPtr<double> X;
  // SparseMatrix<uint32, double>
};


// // use matlab/diff_*.m to check
// TEST_F(SparseMatrixTest, LoadFromRecordIOMultiGroup) {
//   std::vector<string> files;
//   for (int i = 0; i < 4; i++)
//     files.push_back("../data/recordio/ctr4m_part_" + to_string(i));
//   auto data = readMatricesFromProto<double>(files);
//   data[0]->writeToBinFile("y");
//   data[1]->writeToBinFile("x");
// }

// TEST_F(SparseMatrixTest, LoadFromRecordIO) {
//   std::vector<string> files(1, "../data/rcv1.recordio");

//   auto data = readMatricesFromProto<double>(files);
//   auto Y = data[1];
//   // Y->writeToBinFile("tmp");
//   for (int i = 0; i < 10; ++i) {
//     Vec w = Vec::Random(X->cols());
//     EXPECT_LE( (*X * w - *Y * w).norm(), 1e-3);
//   }
// }

// TEST_F(SparseMatrixTest, Times) {
//   Vec a = Vec::Ones(47236) * 2;
//   auto b = *X * a;
//   EXPECT_EQ((int)(b.norm()*100000), 208710928);

//   Vec c = Vec::Ones(20242) * 2;
//   auto d = *X->trans() * c;
//   EXPECT_EQ((int)(d.norm()*100000), 558041409);
// }



// TEST_F(SparseMatrixTest, Localize) {
//   SArray<Key> key;
//   auto Y = X->localize(&key);
//   EXPECT_EQ((int)Y->cols(), 44504);

//   size_t sk = 0;
//   for (auto k : key) sk += k;
//   EXPECT_EQ(1051814869, sk);

//   Vec a = Vec::Ones(44504) * 2;
//   auto b = *Y * a;
//   EXPECT_EQ((int)(b.norm()*100000), 208710928);

//   Vec c = Vec::Ones(20242) * 2;
//   auto d = Y->transTimes(c);
//   EXPECT_EQ((int)(d.norm()*100000), 558041409);

//   // LL << Y.info().DebugString();
// }

// TEST_F(SparseMatrixTest, LocalizeBigKey) {
//   SArray<Key> key;
//   auto Y = std::static_pointer_cast<SM>(X)->localizeBigKey2(&key);
//   EXPECT_EQ((int)Y->cols(), 44504);

//   size_t sk = 0;
//   for (auto k : key) sk += k;
//   EXPECT_EQ(1051814869, sk);

//   Vec a = Vec::Ones(44504) * 2;
//   auto b = *Y * a;
//   EXPECT_EQ((int)(b.norm()*100000), 208710928);

//   Vec c = Vec::Ones(20242) * 2;
//   auto d = Y->transTimes(c);
//   EXPECT_EQ((int)(d.norm()*100000), 558041409);

//   // LL << Y.info().DebugString();
// }

// TEST_F(SparseMatrixTest, AlterStorage) {
//   auto Y = X->toColMajor();
//   {
//     auto Z = X->toRowMajor();
//   }
//   for (int i = 0; i < 10; ++i) {
//     Vec w = Vec::Random(Y->cols());
//     EXPECT_LE( (*X * w - *Y * w).norm(), 1e-6);
//   }

//   Vec a = Vec::Ones(47236) * 2;
//   auto b = *Y * a;
//   EXPECT_EQ((int)(b.norm()*100000), 208710928);

//   Vec c = Vec::Ones(20242) * 2;
//   auto d = *Y->trans() * c;
//   EXPECT_EQ((int)(d.norm()*100000), 558041409);
// }

// TEST_F(SparseMatrixTest, RowBlock) {
//   SizeR row(0, X->rows());
//   int num_block = 10;
//   for (int j = 0; j < 5; ++j) {
//     Vec w = Vec::Random(X->cols());
//     Vec Xw = Vec::Zero(X->rows());

//     for (int i = 0; i < num_block; ++i) {
//       auto block = row.evenDivide(num_block, i);
//       Xw.segment(block.begin(), block.size()) += *(X->rowBlock(block)) * w;
//     }
//     EXPECT_LE( (*X * w - Xw).norm(), 1e-6  );
//   }
// }

// TEST_F(SparseMatrixTest, LoadRowBlock) {
//   for (int i = 0; i < 10; ++i) {
//     SizeR rg(i*1000, (i+1)*1000);
//     auto Y = readMatrixFromBin<double>(rg, "../data/bin/rcv1_X");
//     auto Z = X->rowBlock(rg);

//     EXPECT_EQ(Y->nnz(), Z->nnz());
//     for (int j = 0; j < 10; ++j) {
//       Vec w = Vec::Random(Y->cols());
//       // EXPECT_LE( (*Y * w - *Z * w).norm(),  0 );
//       EXPECT_EQ((*Y*w).norm(), (*Z*w).norm());
//     }
//   }
// }

// TEST_F(SparseMatrixTest, ColBlock) {
//   SizeR col(0, X->cols());
//   int num_block = 10;
//   auto Y = X->toColMajor();
//   for (int j = 0; j < 5; ++j) {
//     Vec w = Vec::Random(X->cols());
//     Vec Xw = Vec::Zero(X->rows());

//     for (int i = 0; i < num_block; ++i) {
//       auto block = col.evenDivide(num_block, i);
//       Xw += *(Y->colBlock(block)) * w.segment(block.begin(), block.size());
//     }
//     // auto block = col.evenDivide(num_block, 0);
//     // Vec Xw = *(Y->colBlock(block)) * w.segment(block.begin(), block.size());
//     EXPECT_LE( (*X * w - Xw).norm(), 1e-6  );
//   }
// }
