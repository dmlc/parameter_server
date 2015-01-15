#include "gtest/gtest.h"
#include "neural_network/auc_layer.h"
#include "util/shared_array_inl.h"

using namespace PS;
using namespace NN;


TEST(AUCLayer, forward) {

  SArray<double> true_y, predict_y;

  SizeR r(0, 1113980);
  true_y.readFromFile(r, "../data/bin/label");
  predict_y.readFromFile(r, "../data/bin/predict");

  AUCLayer<double> auc;

  ParameterPtr<double> b(new Parameter<double>("b"));
  b->value = predict_y.matrix();
  auc.inArgs().push_back(b);

  ParameterPtr<double> a(new Parameter<double>("a"));
  a->value = true_y.matrix();
  auc.inArgs().push_back(a);

  double v = auc.forward();

  LL << v;
}
