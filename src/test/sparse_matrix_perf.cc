#include "gtest/gtest.h"

using namespace PS;
typedef Eigen::Matrix<double, Eigen::Dynamic, 1> Vec;
typedef SparseMatrixPtr<uint64, double> SpMat;

class SparseMatrixPerf : public ::testing::Test {
 protected:
  virtual void SetUp() {
    DataConfig data;
    data.set_format(DataConfig::PROTO);
    data.add_file("../../data/ctrb/train/part-000[0-3].*");

    auto list = readMatricesOrDie<double>(searchFiles(data));
    X = std::static_pointer_cast<SparseMatrix<uint64, double>>(list[1]);

    FLAGS_num_threads = 1;
    auto t = tic();
    X->countUniqIndex(&key);
    LL << "sort keys: " << toc(t);

    time.resize(num_threads+1);
  }

  SArray<uint64> key;
  SpMat X;
  int num_threads = 4;
  std::vector<double> time;
};


// typedef SparseMatrix<int32, double> SM;
// typedef Eigen::Matrix<double, Eigen::Dynamic, 1> Vec;


// class SparseMatrixPerf : public ::testing::Test {
//  protected:
//   virtual void SetUp() {
//     X = readMatrixFromBin<double>("../../data/bin/ctr4mpv1");
//     // X = readMatrixFromBin<double>("../data/bin/rcv1_X");
//     time.resize(num_threads+1);
//   }
//   MatrixPtr<double> X;
//   int num_threads = 4;
//   std::vector<double> time;
// };

TEST_F(SparseMatrixPerf, TimesByRemaping) {

  auto t2 = tic();
  auto Y = X->remapIndex(key);
  LL << "remap matrix: " << toc(t2);

  FLAGS_num_threads = 1;
  Vec a = Vec::Random(Y->cols());
  auto t1 = tic();
  LL << (*Y * a).norm();
  LL << "mul: " << toc(t1);
}


TEST_F(SparseMatrixPerf, TimesByHash) {

  auto t2 = tic();
  std::default_random_engine generator;
  std::normal_distribution<double> distribution(0, 1);
  std::unordered_map<uint64, double> w;
  for (auto k : key) w[k] = distribution(generator);
  LL << "build w: " << toc(t2);
  SArray<double> b(X->rows());

  auto t = tic();
  auto os = X->offset();
  auto idx = X->index();

  for (int i = 0; i < os.size() - 1; ++i) {
    double v = 0;
    for (size_t j = os[i]; j < os[i+1]; ++j) {
      v += w[idx[j]];
    }
    b[i] = v;
  }

  LL << "mul: " << toc(t);
}


//   {

//   }
//   // return;
//   Vec b = Vec::Random(X->rows());

//   std::vector<double> a_norm, b_norm,
//       a_time(num_threads+1), b_time(num_threads+1);

//   // warm up
//   a_norm.push_back((*X * a).norm());
//   b_norm.push_back((*X->trans() * b).norm());

//   for (int i = 1; i <= num_threads; ++i) {
//     FLAGS_num_threads = i;
//     {
//       ScopedTimer t(a_time.data() + i);
//       a_norm.push_back((*X * a).norm());
//     }
//     {
//       ScopedTimer t(b_time.data() + i);
//       b_norm.push_back((*X->trans() * b).norm());
//     }
//     EXPECT_EQ(a_norm[i], a_norm[0]);
//     EXPECT_EQ(b_norm[i], b_norm[0]);
//     if (i > 1) {
//       std::cerr << i << " threads, speedup of times: " << a_time[1] / a_time[i]
//                 << ",\ttrans_times: " << b_time[1] / b_time[i] << std::endl;
//     } else {
//       std::cerr << "single threads: times: " << a_time[1] << ", trans_times " << b_time[1]<< std::endl;
//     }
//   }
// }

// TEST_F(SparseMatrixPerf, AlterStorage) {
//   return;
//   Vec w = Vec::Random(X->cols());
//   Vec res;
//   // warm up
//   {
//     auto Z = X->toColMajor();
//     res = *Z * w;
//   }

//   for (int i = 1; i <= num_threads; ++i) {
//     FLAGS_num_threads = i;
//     MatrixPtr<double> Z;
//     {
//       ScopedTimer t(time.data() + i);
//       Z = X->toColMajor();
//     }
//     FLAGS_num_threads = 4;
//     EXPECT_LE( (*Z * w - res).norm(), 1e-6);
//     if (i > 1) {
//       std::cerr << i << " threads, speedup of alter_storage "
//                 << time[1] / time[i]<< std::endl;
//     }
//   }
// }

// TEST_F(SparseMatrixPerf, Localize) {
//   return;
//   SArray<Key> key;
//   Vec w, res, key_sum;
//   Key sum;
//   // warm up
//   {
//     Timer t; t.start();
//     auto Y = X->localize(&key);
//     w = Vec::Random(Y->cols());
//     res = *Y * w;
//     sum = key.eigenVector().sum();
//     LL << "single thread: " << t.get();
//   }

//   for (int i = 1; i <= num_threads; ++i) {
//     FLAGS_num_threads = i;
//     MatrixPtr<double> Y;
//     {
//       ScopedTimer t(time.data() + i);
//       Y = X->localize(&key);
//     }
//     FLAGS_num_threads = 4;
//     EXPECT_LE( (*Y * w - res).norm(), 1e-6);
//     EXPECT_EQ(sum, key.eigenVector().sum());
//     if (i > 1) {
//       std::cerr << i << " threads, speedup of localize "
//                 << time[1] / time[i]<< std::endl;
//     }
//   }
// }

// TEST_F(SparseMatrixPerf, LocalizeBigKey) {

//   return;
//   SArray<Key> key;
//   Vec w, res, key_sum;
//   Key sum;
//   // warm up

//   auto data = readMatricesFromProto<double>(std::vector<std::string>(1,"../data/recordio/adfea/part-02000"));
//   auto Z = std::static_pointer_cast<SparseMatrix<uint64, double> >(data[1]);
//   {
//     Timer t; t.start();
//     auto Y = Z->localizeBigKey(&key);
//     w = Vec::Random(Y->cols());
//     res = *Y * w;
//     sum = key.eigenVector().sum();
//     LL << FLAGS_num_threads << " threads: " << t.get();
//   }

//   for (int i = 1; i <= num_threads; ++i) {
//     FLAGS_num_threads = i;
//     MatrixPtr<double> Y;
//     {
//       ScopedTimer t(time.data() + i);
//       Y = Z->localizeBigKey(&key);
//     }
//     FLAGS_num_threads = 4;
//     EXPECT_LE( (*Y * w - res).norm(), 1e-6);
//     EXPECT_EQ(sum, key.eigenVector().sum());
//     if (i > 1) {
//       std::cerr << i << " threads, speedup of localize "
//                 << time[1] / time[i]<< std::endl;
//     }
//   }
// }
