#include "gtest/gtest.h"
#include "base/matrix_io.h"
// #include "loss/loss_factory.h"
// #include "learner/learner_factory.h"
// #include "penalty/penalty_factory.h"

using namespace PS;

class AggGradLearnerTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    readFileToProtoOrDie("../src/test/aggregated_gradient_test.config", &cf);
    MatrixPtrList<double> training = readMatrices<double>(cf.training());
    y = training[0];
    X = training[1];
    w.resize(X->cols()); w.setZero();
    Xw.resize(X->rows()); Xw.setZero();
    loss = LossFactory<double>::create(cf.loss());
    penalty = PenaltyFactory<double>::create(cf.penalty());
  }
  AppConfig cf;
  MatrixPtr<double> X, y;
  SArray<double> w, Xw;
  LossPtr<double> loss;
  PenaltyPtr<double> penalty;
  SArrayList<double> grads;
  AggGradLearnerPtr<double> learner;
};

// use test/graddesc.m to produce the "correct" answer
TEST_F(AggGradLearnerTest, GradientDescent) {
  cf.mutable_learner()->set_type(LearnerConfig::GRADIENT_DESCENT);
  learner = std::static_pointer_cast<AggGradLearner<double>>(
      LearnerFactory<double>::create(cf.learner()));
  learner->setLoss(loss);

  grads.resize(1); grads[0].resize(w.size());

  for (int i = 0; i < 11; ++i) {
    AggGradLearnerArg arg; arg.set_learning_rate(1);

    Xw.vec() = *X * w.vec();
    learner->compute({y, X, Xw.matrix()}, arg, grads);
    learner->update(grads, arg, w);

    double fval = loss->evaluate({y, Xw.matrix()});
    if (i == 2) EXPECT_LE(fabs(fval-10786), 1.0);
    if (i == 10) EXPECT_LE(fabs(fval-110360), 1.0);
  }
}

// liblinear results:
// load rcv1,  w = train(Y,X,'-s 6');
// iter   1  #CD cycles 1
// iter   2  #CD cycles 1
// iter   3  #CD cycles 1
// iter   4  #CD cycles 1
// iter   5  #CD cycles 3
// =========================
// optimization finished, #iter = 5
// Objective value = 3918.901322
// #nonzeros/#features = 579/47236
TEST_F(AggGradLearnerTest, BlockCoordinateDescent) {
  cf.mutable_learner()->set_type(LearnerConfig::PROXIMAL_GRADIENT);
  learner = std::static_pointer_cast<AggGradLearner<double>>(
      LearnerFactory<double>::create(cf.learner()));
  learner->setLoss(loss);
  learner->setPenalty(penalty);

  grads.resize(2);

  int num_block = 50;
  auto tX = X->toColMajor();

  // auto tv = tic();
  SArray<double> w_old;
  for (int i = 0; i < 11; ++i) {
    AggGradLearnerArg arg; arg.set_learning_rate(1);
    for (int b = 0; b < num_block; ++b) {
      auto block = SizeR(0, tX->cols()).evenDivide(num_block, b);
      auto bX = tX->colBlock(block);
      auto bw = w.segment(block);

      grads[0].resize(bw.size());
      grads[1].resize(bw.size());

      learner->compute({y, bX, Xw.matrix()}, arg, grads);
      w_old.copyFrom(bw);
      learner->update(grads, arg, bw);
      Xw.vec() += *bX * (bw.vec() - w_old.vec());
    }

    double fval = loss->evaluate({y, Xw.matrix()}) + penalty->evaluate(w.matrix());

    // LL << i << " " << fval << " " << w.nnz();
    if (i == 2) {
      EXPECT_LE(fabs(fval-4136), 1.0);
      EXPECT_EQ(w.nnz(), 654);
    }
    if (i == 10) {
      EXPECT_LE(fabs(fval-3931), 1.0);
      EXPECT_EQ(w.nnz(), 572);
    }
  }

  // LL << toc(tv);
  // std::ofstream out("/tmp/w");
  // for (auto v : w) out << v << std::endl;

}

// TEST(GradDesc, tmp) {
//   SArray<double> r;
//   LL << r.array().size();
//   LL << r.vec().size();

//   // LL << r.vec().empty();
//   LL << SArray<double>().array().size();
// }
