#include "gtest/gtest.h"
#include "base/matrix_io_inl.h"
#include "linear_method/loss_inl.h"
#include "linear_method/linear_method.pb.h"
#include "data/stream_reader.h"
#include "base/localizer.h"

using namespace PS;
using namespace LM;
TEST(FTRL, LogistRegression) {
  typedef float V;
  // load data
  Config cf; readFileToProtoOrDie("../src/test/grad_desc.conf", &cf);
  StreamReader<V> reader(cf.training_data());
  MatrixPtrList<V> X;
  while (reader.readMatrices(100, &X)) {

  // LL << X.size();

  // LL << X[0]->debugString();
  // LL << X[1]->debugString();


  Localizer<Key, V> lc;
  SArray<Key> uniq_idx;
  SArray<uint32> idx_frq;

  lc.countUniqIndex(X[1], &uniq_idx, &idx_frq);
  auto Z = lc.remapIndex(uniq_idx);
  LL << Z->debugString();
  }

  struct Entry {
    uint32 pos_count = 0;
    uint32 neg_count = 0;
    double weight = 0;
  };
  std::unordered_map<Key, Entry> model;





  // // allocate memories
  // SArray<V> w(X->cols()), Xw(X->rows()),  g(X->cols());
  // w.setZero();

  // auto loss = Loss<V>::create(cf.loss());

  // V eta = cf.learning_rate().eta();
  // for (int i = 0; i < 11; ++i) {
  //   Xw.eigenArray() = *X * w.eigenArray();
  //   loss->compute({data[0], data[1], Xw.matrix()}, {g.matrix()});
  //   w.eigenArray() -= eta * g.eigenArray();

  //   V objv = loss->evaluate({data[0], Xw.matrix()});
  //   LL << objv;
  // }
}
