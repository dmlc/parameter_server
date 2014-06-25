#include "gtest/gtest.h"
#include "base/matrix_io.h"
#include "loss/loss_factory.h"
#include "learner/learner_factory.h"

using namespace PS;
TEST(GradientDescent, LogistRegression) {
  AppConfig cf; ReadFileToProtoOrDie("../test/grad_desc.config", &cf);
  MatrixPtrList<double> training = readMatrices<double>(cf.training());

  auto X = training[1];
  SArray<double> w(X->cols()); w.setZero();
  SArray<double> Xw(X->rows());

  double eta = 1;


  auto loss = LossFactory<double>::create(cf.loss());
  auto learner = LearnerFactory<double>::create(cf.learner());

  // for (int i = 0; i < 11; ++i) {
  //   Xw.vec() = *X * w.vec();

  //   auto grad = loss->
  //   Xw.vec() = *X * w;
  //   auto grad = loss->gradient(Y, Xw, X);
  //   w -= eta * grad.vec();
  //   double fval = loss->value(Y, Xw);
  //   if (i == 2) EXPECT_LE(fabs(fval-10786), 1.0);
  //   if (i == 10) EXPECT_LE(fabs(fval-110360), 1.0);
  // }

}

// TEST(GradDesc, tmp) {
//   SArray<double> r;
//   LL << r.array().size();
//   LL << r.vec().size();

//   // LL << r.vec().empty();
//   LL << SArray<double>().array().size();
// }
