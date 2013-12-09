#include "algo/sgd.h"
#include "box/vectors.h"
#include "box/item.h"

namespace PS {

DEFINE_int32(mini_batch, 20, "mini batch size");

void SGD::Client() {
  // load data
  LoadXY(FLAGS_train_data, data_range_.start(), data_range_.end(), &Y_, &X_);
  LL << SName() << StrCat(" load ", FLAGS_train_data, " ", data_range_.ToString());
  Vector<double> W("sgd_w", num_total_feature_);
  W.SetMaxDelay(FLAGS_max_push_delay, FLAGS_max_pull_delay);

  Item<Progress> Obj("objv");
  Obj.SetMaxDelay(1,1);

  double eta = FLAGS_eta;
  int num_sample = X_.rows();
  // W.vec() = DVec::Random(num_total_feature_);
  for (int iter = 0; iter < 100; ++iter) {
    // mini-batch
    for (int b = 0; b < FLAGS_mini_batch; ++b) {
      int sample_id = rand() % num_sample;
      const auto& x= X_.row(sample_id);
      double y = Y_[sample_id];
      double dual = -y / (1 + exp(x.dot(W.vec())*y));
      W.vec() -= eta * dual * x.adjoint();
    }
    // do push
    Header flag;
    flag.set_recver(NodeGroup::kServers);
    flag.set_type(Header::PUSH_PULL);
    flag.set_push_delta(true);
    flag.set_pull_delta(false);
    // W.Push(flag);

    // calculate objective value
    Progress prog;
    // sum(log(1+exp(-y.*(x'*w))));
    prog.objv = ((-Y_.cwiseProduct(X_*W.vec())).array().exp() + 1).log().sum();
    DVec pred_Y = ((X_*W.vec()).array().exp() + 1).inverse();
    prog.err = ClassifyError(Y_, pred_Y);

    // do communication
    Obj.data() = prog;
    std::shared_future<Progress> fut;
    Obj.AllReduce(&fut);
    auto v = fut.get();
    // Obj.AllReduce();
    // auto v = Obj.data();
    double train_err = 1 - (double)v.err / (double) num_total_sample_;
    if (FLAGS_my_rank == 0)
      LL << "iter " << iter << "\tobjv: " << v.objv << "\ttrain err: " << train_err;

  }
}

void SGD::Server() {
  Vector<double> W("sgd_w", num_total_feature_);
  Item<Progress> Obj("objv");
  Obj.SetAggregator(NodeGroup::kClients);
  sleep(30);
  // LL << "init " << my_node_name();
  // DenseVector<double> w("sgd_w", num_total_feature_);
  // w.set_best_effort();
  // while(1) { }

}

} // namespace PS
