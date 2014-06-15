#include "gtest/gtest.h"
#include "base/matrix_io.h"

using namespace PS;

typedef Eigen::Matrix<double, Eigen::Dynamic, 1> Vec;


class SparseMatrixPerf : public ::testing::Test {
 protected:
  virtual void SetUp() {
    X = readMatrixFromBin<double>("/home/muli/work/data/bin/ctr4mpv1");
    time.resize(num_threads+1);
  }
  MatrixPtr<double> X;
  int num_threads = 4;
  std::vector<double> time;
};

TEST_F(SparseMatrixPerf, Times) {
  return;
  Vec a = Vec::Random(X->cols());
  Vec b = Vec::Random(X->rows());

  std::vector<double> a_norm, b_norm,
      a_time(num_threads+1), b_time(num_threads+1);

  // warm up
  a_norm.push_back((*X * a).norm());
  b_norm.push_back((*X->trans() * b).norm());

  for (int i = 1; i <= num_threads; ++i) {
    FLAGS_num_threads = i;
    {
      ScopedTimer t(a_time.data() + i);
      a_norm.push_back((*X * a).norm());
    }
    {
      ScopedTimer t(b_time.data() + i);
      b_norm.push_back((*X->trans() * b).norm());
    }
    EXPECT_EQ(a_norm[i], a_norm[0]);
    EXPECT_EQ(b_norm[i], b_norm[0]);
    if (i > 1) {
      std::cerr << i << " threads, speedup of times: " << a_time[1] / a_time[i]
                << ",\ttrans_times: " << b_time[1] / b_time[i] << std::endl;
    }
  }
}

TEST_F(SparseMatrixPerf, AlterStorage) {
  return;
  Vec w = Vec::Random(X->cols());
  Vec res;
  // warm up
  {
    auto Z = X->toColMajor();
    res = *Z * w;
  }

  for (int i = 1; i <= num_threads; ++i) {
    FLAGS_num_threads = i;
    MatrixPtr<double> Z;
    {
      ScopedTimer t(time.data() + i);
      Z = X->toColMajor();
    }
    FLAGS_num_threads = 4;
    EXPECT_LE( (*Z * w - res).norm(), 1e-6);
    if (i > 1) {
      std::cerr << i << " threads, speedup of alter_storage "
                << time[1] / time[i]<< std::endl;
    }
  }
}

TEST_F(SparseMatrixPerf, Localize) {

  SArray<Key> key;
  Vec w, res, key_sum;
  Key sum;
  // warm up
  {
    auto Y = X->localize(&key);
    w = Vec::Random(Y->cols());
    res = *Y * w;
    sum = key.vec().sum();
  }

  for (int i = 1; i <= num_threads; ++i) {
    FLAGS_num_threads = i;
    MatrixPtr<double> Y;
    {
      ScopedTimer t(time.data() + i);
      Y = X->localize(&key);
    }
    FLAGS_num_threads = 4;
    EXPECT_LE( (*Y * w - res).norm(), 1e-6);
    EXPECT_EQ(sum, key.vec().sum());
    if (i > 1) {
      std::cerr << i << " threads, speedup of localize "
                << time[1] / time[i]<< std::endl;
    }
  }
}
