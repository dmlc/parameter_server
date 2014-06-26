#include "app/block_coordinate_l1lr.h"
#include "base/sparse_matrix.h"
namespace PS {

// quite similar to LinearBlockIterator::run(), but diffs at the KKT filter
void BlockCoordinateL1LR::run() {
  LinearMethod::startSystem();
  auto blocks = LinearBlockIterator::partitionFeatures();

  std::vector<int> block_order;
  for (int i = 0; i < blocks.size(); ++i) block_order.push_back(i++);
  auto cf = app_cf_.block_iterator();

  // iterating
  auto wk = taskpool(kActiveGroup);
  int time = wk->time();
  int tau = cf.max_block_delay();
  for (int iter = 0; iter < cf.max_pass_of_data(); ++iter) {
    std::random_shuffle(block_order.begin(), block_order.end());

    for (int b : block_order)  {
      Task update;
      auto cmd = RiskMinimization::setCall(&update);
      update.set_wait_time(time - tau);
      cmd->set_cmd(RiskMinCall::UPDATE_MODEL);
      blocks[b].second.to(cmd->mutable_key());
      cmd->set_feature_group_id(blocks[b].first);
      time = wk->submit(update);
    }

    Task eval;
    RiskMinimization::setCall(&eval)->set_cmd(RiskMinCall::EVALUATE_PROGRESS);
    eval.set_wait_time(time - tau);

    time = wk->submit(eval, [this, iter](){ RiskMinimization::mergeProgress(iter); });
    wk->waitOutgoingTask(time);

    RiskMinimization::showProgress(iter);
    if (global_progress_[iter].relative_objv() <= cf.epsilon()) {
      fprintf(stderr, "convergence criteria satisfied: relative objective <= %.1e\n", cf.epsilon());
      break;
    }
  }

}

void BlockCoordinateL1LR::prepareData(const Message& msg) {
  LinearBlockIterator::prepareData(msg);
  if (exec_.isWorker()) {
    // dual_ = exp(y.*(X_*w_))
    dual_.array() = exp(y_->value().array() * dual_.array());
  }

  active_set_.resize(w_->size(), true);
  delta_.resize(w_->size());
  delta_.setValue(app_cf_.block_coord_l1lr().delta_init_value());
}

void BlockCoordinateL1LR::updateModel(Message* msg) {

  CHECK(false);
  auto time = msg->task.time() * 10;
  Range<Key> global_range(msg->task.risk().key());
  auto local_range = w_->localRange(global_range);
  int num_threads = FLAGS_num_threads;

  if (exec_.isWorker()) {

    // compute local gradients
    SArrayList<double> local_grads(2);
    for (int i : {0, 1} ) {
      local_grads[i].resize(local_range.size());
      local_grads[i].setZero();
    }
    // local_grads[1].resize(local_range.size());


    {
      Lock lk(mu_);
      computeGradients(local_range, local_grads[0], local_grads[1]);
    }

    msg->finished = false;
    auto d = *msg;
    // w_->roundTripForWorker(time, global_range, local_grads, [this, X, local_range, d] (int time) {
    //     // update dual variable
    //     Lock l(mu_);
    //     busy_timer_.start();

    //     if (!local_range.empty()) {
    //       auto data = w_->received(time);

    //       CHECK_EQ(data.size(), 1);
    //       CHECK_EQ(local_range, data[0].first);
    //       auto new_w = data[0].second;

    //       auto delta = new_w.vec() - w_->segment(local_range).vec();
    //       dual_.vec() += *X * delta;
    //       w_->segment(local_range).vec() = new_w.vec();
    //     }

    //     busy_timer_.stop();
    //     taskpool(d.sender)->finishIncomingTask(d.task.time());
    //     sys_.reply(d);
    //     // LL << myNodeID() << " done " << d.task.time();
    //   });
  } else {
    // aggregate local gradients, then update model
    w_->roundTripForServer(time, global_range, [this, local_range] (int time) {
        SArrayList<double> aggregated_gradient;
        for (auto& d : w_->received(time)) {
          CHECK_EQ(local_range, d.first);
          aggregated_gradient.push_back(d.second);
        }
        AggGradLearnerArg arg;
        arg.set_learning_rate(app_cf_.learner().learning_rate());

        learner_->update(aggregated_gradient, arg, w_->segment(local_range));
      });
  }
}

void BlockCoordinateL1LR::computeGradients(
    SizeR local_feature_range, SArray<double> G, SArray<double> U) {
  CHECK_EQ(G.size(), local_feature_range.size());
  CHECK_EQ(U.size(), local_feature_range.size());
  CHECK(!X_->rowMajor());

  auto X = std::static_pointer_cast<SparseMatrix<Key,double>>(X_->colBlock(local_feature_range));
  const auto& offset = X->offset();
  bool binary = X->binary();

  // j: column id, i: row id
  for (size_t j = 0; j < offset.size()-1; ++j) {
    size_t k  = j + local_feature_range.begin();
    if (!active_set_.test(k) || offset[j] == offset[j+1]) continue;
    double g = 0, u = 0, d = delta_[k];
    for (size_t o = offset[j]; o < offset[j+1]; ++o) {
      auto i = X->index()[o];
      double tau = 1 / ( 1 + dual_[i] );
      if (binary) {
        g -= tau;
        u += std::min(tau*(1-tau)*exp(d), .25);
      } else {
        double v = X->value()[o];
        g -= tau * v;
        u += std::min(tau*(1-tau)*exp(fabs(d*v)), .25) * v * v;
      }
    }
    G[j] = g;
    U[j] = u;
  }
}


void BlockCoordinateL1LR::updateDual(
    SizeR local_example_range, SizeR local_feature_range,
    SArray<double> w_delta) {
  CHECK_EQ(w_delta.size(), local_feature_range.size());
  CHECK(!X_->rowMajor());

  auto X = std::static_pointer_cast<SparseMatrix<Key,double>>(X_->colBlock(local_feature_range));
  const auto& y = y_->value();
  const auto& offset = X->offset();
  bool binary = X->binary();

  // j: column id, i: row id
  for (size_t j = 0; j < offset.size()-1; ++j) {
    size_t k  = j + local_feature_range.begin();
    if (!active_set_.test(k) || offset[j] == offset[j+1]) continue;
    double wd = w_delta[j];

    for (size_t o = offset[j]; o < offset[j+1]; ++o) {
      auto i = X->index()[o];
      if (!local_example_range.contains(i)) continue;
      dual_[i] *= binary ? exp(y[i] * wd) : exp(y[i] * wd * X->value()[o]);
    }
  }
}

} // namespace PS
