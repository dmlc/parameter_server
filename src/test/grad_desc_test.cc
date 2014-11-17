#include "gtest/gtest.h"
#include "base/matrix_io_inl.h"
#include "linear_method/loss_inl.h"
#include "linear_method/linear_method.pb.h"

using namespace PS;
using namespace LM;
TEST(GradientDescent, LogistRegression) {
  // load data
  typedef double V;
  Config cf; readFileToProtoOrDie("../src/test/grad_desc.conf", &cf);
  auto data = readMatricesOrDie<V>(cf.training_data());
  CHECK_EQ(data.size(), 2);
  auto X = data[1];

  // allocate memories
  SArray<V> w(X->cols()), Xw(X->rows()),  g(X->cols());
  w.setZero();

  auto loss = Loss<V>::create(cf.loss());

  V eta = cf.learning_rate().eta();
  for (int i = 0; i < 11; ++i) {
    Xw.eigenArray() = *X * w.eigenArray();
    loss->compute({data[0], data[1], Xw.matrix()}, {g.matrix()});
    w.eigenArray() -= eta * g.eigenArray();

    V objv = loss->evaluate({data[0], Xw.matrix()});
    LL << objv;
  }
}
