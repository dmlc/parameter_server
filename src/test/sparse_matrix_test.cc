#include "gtest/gtest.h"
#include "base/matrix_io.h"

using namespace PS;

// typedef SparseMatrix<int32, double> SM;
typedef Eigen::Matrix<double, Eigen::Dynamic, 1> Vec;

class SparseMatrixTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    X = readMatrixFromBin<double>("../data/bin/rcv1_X");
  }
  MatrixPtr<double> X;
};

// TEST(SparseMatrix, Info) {

//   MatrixInfo info;
//   info.set_type(MatrixInfo::SPARSE);
//   info.mutable_row()->set_begin(100);
//   // LL << info.DebugString();
// }

TEST_F(SparseMatrixTest, LoadFromRecordIOMultiGroup) {
  std::vector<string> files;
  // for (int i = 0; i < 1; i++)
  int i = 3;
    files.push_back("../data/recordio/ctr4m_part_" + to_string(i));

  auto data = readMatricesFromProto<double>(files);
  auto Y = data[1];
  // Y->writeToBinFile("../data/tmp");
  // for (int i = 0; i < 10; ++i) {
  //   Vec w = Vec::Random(X->cols());
  //   EXPECT_LE( (*X * w - *Y * w).norm(), 1e-3);
  // }
}

TEST_F(SparseMatrixTest, LoadFromRecordIO) {
  std::vector<string> files;
  for (int i = 0; i < 4; i++)
    files.push_back("../data/recordio/rcv1_part_" + to_string(i));

  auto data = readMatricesFromProto<double>(files);
  auto Y = data[1];
  // Y->writeToBinFile("../data/tmp");
  for (int i = 0; i < 10; ++i) {
    Vec w = Vec::Random(X->cols());
    EXPECT_LE( (*X * w - *Y * w).norm(), 1e-3);
  }
}

TEST_F(SparseMatrixTest, Times) {
  Vec a = Vec::Ones(47236) * 2;
  auto b = *X * a;
  EXPECT_EQ((int)(b.norm()*100000), 208710928);

  Vec c = Vec::Ones(20242) * 2;
  auto d = *X->trans() * c;
  EXPECT_EQ((int)(d.norm()*100000), 558041409);
}



TEST_F(SparseMatrixTest, Localize) {
  SArray<Key> key;
  auto Y = X->localize(&key);
  EXPECT_EQ((int)Y->cols(), 44504);

  size_t sk = 0;
  for (auto k : key) sk += k;
  EXPECT_EQ(1051814869, sk);

  Vec a = Vec::Ones(44504) * 2;
  auto b = *Y * a;
  EXPECT_EQ((int)(b.norm()*100000), 208710928);

  Vec c = Vec::Ones(20242) * 2;
  auto d = Y->transTimes(c);
  EXPECT_EQ((int)(d.norm()*100000), 558041409);

  // LL << Y.info().DebugString();
}

TEST_F(SparseMatrixTest, AlterStorage) {
  auto Y = X->toColMajor();
  {
    auto Z = X->toRowMajor();
  }
  for (int i = 0; i < 10; ++i) {
    Vec w = Vec::Random(Y->cols());
    EXPECT_LE( (*X * w - *Y * w).norm(), 1e-6);
  }

  Vec a = Vec::Ones(47236) * 2;
  auto b = *Y * a;
  EXPECT_EQ((int)(b.norm()*100000), 208710928);

  Vec c = Vec::Ones(20242) * 2;
  auto d = *Y->trans() * c;
  EXPECT_EQ((int)(d.norm()*100000), 558041409);
}

TEST_F(SparseMatrixTest, RowBlock) {
  SizeR row(0, X->rows());
  int num_block = 10;
  for (int j = 0; j < 5; ++j) {
    Vec w = Vec::Random(X->cols());
    Vec Xw = Vec::Zero(X->rows());

    for (int i = 0; i < num_block; ++i) {
      auto block = row.evenDivide(num_block, i);
      Xw.segment(block.begin(), block.size()) += *(X->rowBlock(block)) * w;
    }
    EXPECT_LE( (*X * w - Xw).norm(), 1e-6  );
  }
}

TEST_F(SparseMatrixTest, LoadRowBlock) {
  for (int i = 0; i < 10; ++i) {
    SizeR rg(i*1000, (i+1)*1000);
    auto Y = readMatrixFromBin<double>(rg, "../data/bin/rcv1_X");
    auto Z = X->rowBlock(rg);

    EXPECT_EQ(Y->nnz(), Z->nnz());
    for (int j = 0; j < 10; ++j) {
      Vec w = Vec::Random(Y->cols());
      // EXPECT_LE( (*Y * w - *Z * w).norm(),  0 );
      EXPECT_EQ((*Y*w).norm(), (*Z*w).norm());
    }
  }
}

TEST_F(SparseMatrixTest, ColBlock) {
  SizeR col(0, X->cols());
  int num_block = 10;
  auto Y = X->toColMajor();
  for (int j = 0; j < 5; ++j) {
    Vec w = Vec::Random(X->cols());
    Vec Xw = Vec::Zero(X->rows());

    for (int i = 0; i < num_block; ++i) {
      auto block = col.evenDivide(num_block, i);
      Xw += *(Y->colBlock(block)) * w.segment(block.begin(), block.size());
    }
    // auto block = col.evenDivide(num_block, 0);
    // Vec Xw = *(Y->colBlock(block)) * w.segment(block.begin(), block.size());
    EXPECT_LE( (*X * w - Xw).norm(), 1e-6  );
  }
}







////////////////////////////////


// TEST_F(SparseMatrixTest, Norm) {
//   EXPECT_LE(X.Norm(1), 596.33);
//   EXPECT_GE(X.Norm(1), 596.32);
//   EXPECT_GE(X.Norm(2), 7.616);
//   EXPECT_LE(X.Norm(2), 7.6161);
// }

// TEST_F(SparseMatrixTest, Times) {
//   DVec b;
//   DVec d = DVec::Random(X.cols());
//   X.TransposeTimes(DVec::Ones(X.rows())*10, d.col(0));
//   EXPECT_LE(fabs(d.sum()-X.Norm(1)*10), 1e-6);
// }

// TEST_F(SparseMatrixTest, RandomPermute) {
//   DVec b;
//   X.Times(rand, b);
//   SMY;
//   Y.Load("util/test_data/200");
//   for (int i = 0; i < 100; ++i) {
//     Y.RandomPermute();
//     DVec c;
//     Y.Times(rand, c);
//     EXPECT_LE(b.norm()-c.norm(), 1e-6);
//   }
// }

// TEST_F(SparseMatrixTest, RowBlock) {
//   DVec v; X.Times(rand, v);
//   for (int i = 0; i < 50; i += 2) {
//     Seg a(i, i+10);
//     SM Y;
//     X.RowBlock(a, &Y);
//     DVec w;
//     Y.Times(rand, w);
//     EXPECT_LE((w-v.segment(a.start(), a.size())).norm(), 1e-6);
//   }
// }

// TEST_F(SparseMatrixTest, Transpose) {
//   SM Y; X.Transpose(&Y);
//   EXPECT_LE(fabs(X.Norm(2) - Y.Norm(2)), 1e-6);
//   DVec a, b;
//   X.Times(rand, a);
//   Y.TransposeTimes(rand, b);
//   EXPECT_LE((a-b).norm(), 1e-6);

//   for (int i = 0; i < 50; i += 2) {
//     Seg s(i, i+10);
// s(rand, a);
//     bY.TransposeTimes(rand, b);
//     EXPECT_LE((a-b).norm(), 1e-6);
//   }
// }

// TEST_F(SparseMatrixTest, VNorm) {
//   Seg s(10,40);
//   SM y; X.RowBlock(s, &y);
//   double res[] = {1.3213, 299.244, 20.3633, 3.1926, 2.2575, 107.829, 6.905, 3.1926};
//   for (int dim = 0; dim < 2; ++dim) {
//     for (int norm = -1; norm < 3; ++norm) {
//       auto v = y.Norm(dim, norm);
//       double r = res[dim*4+norm+1];
//       EXPECT_LE(v.norm(), r+1e-3);
//       EXPECT_GE(v.norm(), r-1e-3);
//     }
//   }
// }

// TEST_F(SparseMatrixTest, Select) {
//   Seg s(10,40);
//   SM y; X.RowBlock(s, &y);
//   auto k = y.Norm(1, 0);
//   SM z; y.Select(1, (k.array() > 1.0), &z);
//   EXPECT_EQ(z.cols(), 277);
//   EXPECT_LE(z.Norm(2), 3.0497+1e-3);
//   EXPECT_GE(z.Norm(2), 3.0497-1e-3);
//   EXPECT_LE(z.Norm(1,1).norm(), 6.8402+1e-3);
//   EXPECT_GE(z.Norm(1,1).norm(), 6.8402-1e-3);
// }
