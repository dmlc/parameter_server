#include "app/gradient_descent/gd.h"
#include "box/vectors-inl.h"
#include "box/item.h"
#include "util/rspmat.h"

namespace PS {

void GD::Client() {


  XArray<Key> keys;

  // 1: weights, 2: gradient
  Range<size_t> fea_range = RSpMat<>::ColSeg(FLAGS_train_data);
  Vectors<double> W("grad_desc", fea_range.size(), 2, keys);
  W.SetMaxPullDelay(0);

  // load data
  LL << SName() << " load " << FLAGS_train_data << " " << data_range_.ToString();
  LL << "gradient descent, logistic regression, eta = " << FLAGS_eta;

  RSpMat<int64, double> X;
  X.Load(FLAGS_train_data, data_range_);
  X.ToEigen3(&X_);
  LoadVector<double>(FLAGS_train_data+".label", data_range_, &Y_);

  // W.SetMaxDelay(FLAGS_max_push_delay, FLAGS_max_pull_delay);
  // W.SetMaxDelay(0,0);
  // Item<Progress> Obj("objv");
  // Obj.SetMaxDelay(0,0);


  for (int iter = 0; iter < 100; ++iter) {
    // x'*(-y./(1+exp(y.*(x*w))));
    W.Vec(1) = X_.adjoint() * ( Y_.cwiseQuotient(
        (Y_.cwiseProduct(X_*W.Vec(0)).array().exp()+1).matrix()));
    W.PushPull(KeyRange::All(), {1}, kValue, {0}, kDelta);
    // W.Vec(0) += FLAGS_eta * W.Vec(1);

    // calculate objective value
    Progress prog;
    // sum(log(1+exp(-y.*(x'*w))));
    prog.objv = ((-Y_.cwiseProduct(X_*W.Vec(0))).array().exp() + 1).log().sum();
    DVec pred_Y = ((X_*W.Vec(0)).array().exp() + 1).inverse();
    prog.err = ClassifyError(Y_, pred_Y);

    // do communication
    // Obj.data() = prog;
    // std::shared_future<Progress> fut;
    // Obj.AllReduce(&fut);
    // auto v = fut.get();
    // // Obj.AllReduce();
    // // auto v = Obj.data();
    double train_err = 1 - (double)prog.err / (double) X_.rows();
    if (FLAGS_my_rank == 0)
      printf("%3d %10.3e %7.3f %10.3e\n",
             iter, prog.objv, train_err, W.Vec(0).norm());

  }
}

void UpdateWeight(Vectors<double> *W) {
  W->Vec(0) += FLAGS_eta * W->Vec(1);
}

void GD::Server() {

  Range<size_t> fea_range = RSpMat<>::ColSeg(FLAGS_train_data);
  Vectors<double> W("grad_desc", fea_range.size(), 2);
  W.SetAggregator(NodeGroup::kClients);
  W.SetAggregatorFunc(NewPermanentCallback(UpdateWeight, &W));

  sleep(20);

  // Vector<double> W("sgd_w", num_total_feature_);
  // Item<Progress> Obj("objv");
  // Obj.SetAggregator(NodeGroup::kClients);
  // sleep(30);
  // LL << "init " << my_node_name();
  // DenseVector<double> w("sgd_w", num_total_feature_);
  // w.set_best_effort();
  // while(1) { }

}

} // namespace PS
