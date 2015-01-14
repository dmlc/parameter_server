#include "gtest/gtest.h"
#include "util/auc.h"
#include "util/shared_array_inl.h"

using namespace PS;

class AUCTEST : public ::testing::Test {
 protected:
  virtual void SetUp() {
    // CHECK(false);
    // y.readFromFile("../data/true_label");
    // predict_y.readFromFile("../data/predict_label");
    SizeR r(0,20242);
    y.readFromFile(r, "./y_W0");
    predict_y.readFromFile(r, "./Xw_W0");
  }
  SArray<double> y, predict_y;
};


TEST_F(AUCTEST, goodness) {

  for (int i = 1; i < 400; i+=100) {
    AUC auc_worker;
    auc_worker.setGoodness(i*10);
    AUCData data;
    auc_worker.compute(y, predict_y, &data);

    AUC auc_root;
    auc_root.merge(data);
    LL << auc_root.evaluate();
  }
}

// TEST_F(AUCTEST, dist) {

//   for (int n = 2; n < 20; n+=3) {
//     AUC auc_root;
//     for (int i = 0; i < n; ++i) {
//       AUC auc_worker;
//       AUCData data;
//       auc_worker.setGoodness(100000);
//       auto range = SizeR(0, y.size()).evenDivide(n, i);
//       auc_worker.compute(y.segment(range), predict_y.segment(range), &data);

//       auc_root.merge(data);
//     }
//     LL << auc_root.evaluate();
//   }
// }
