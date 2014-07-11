#include "app/linear_block_iterator.h"
#include "base/matrix_io.h"
namespace PS {

void LinearBlockIterator::run() {
  LinearMethod::startSystem();

  FeatureBlocks blocks = partitionFeatures();
  std::vector<int> block_order;
  for (int i = 0; i < blocks.size(); ++i) block_order.push_back(i);
  auto cf = app_cf_.block_iterator();

  // iterating
  auto pool = taskpool(kActiveGroup);
  int time = pool->time();
  int tau = cf.max_block_delay();
  for (int iter = 0; iter < cf.max_pass_of_data(); ++iter) {
    if (cf.random_feature_block_order())
      std::random_shuffle(block_order.begin(), block_order.end());

    for (int b : block_order)  {
      Task update;
      update.set_wait_time(time - tau);
      auto cmd = RiskMinimization::setCall(&update);
      cmd->set_cmd(RiskMinCall::UPDATE_MODEL);
      // set the feature key range will be updated in this block
      blocks[b].second.to(cmd->mutable_key());
      time = pool->submit(update);
    }

    Task eval;
    RiskMinimization::setCall(&eval)->set_cmd(RiskMinCall::EVALUATE_PROGRESS);
    eval.set_wait_time(time - tau);

    time = pool->submit(eval, [this, iter](){ RiskMinimization::mergeProgress(iter); });
    pool->waitOutgoingTask(time);

    showProgress(iter);
    if (fabs(global_progress_[iter].relative_objv()) <= cf.epsilon()) {
      fprintf(stderr, "convergence criteria satisfied: relative objective <= %.1e\n", cf.epsilon());
      break;
    }
  }

  Task save_model;
  RiskMinimization::setCall(&save_model)->set_cmd(RiskMinCall::SAVE_MODEL);
  time = pool->submit(save_model);
  pool->waitOutgoingTask(time);
}

void LinearBlockIterator::showProgress(int iter) {
  int s = iter == 0 ? -3 : iter;
  for (int i = s; i <= iter; ++i) {
    RiskMinimization::showObjective(i);
    RiskMinimization::showNNZ(i);
    RiskMinimization::showTime(i);
  }
}

LinearBlockIterator::FeatureBlocks LinearBlockIterator::partitionFeatures() {
  FeatureBlocks blocks;
  CHECK(app_cf_.has_block_iterator());
  auto cf = app_cf_.block_iterator();
  if (cf.feature_block_ratio() <= 0) {
    blocks.push_back(std::make_pair(-1, global_training_feature_range_));
  } else {
    for (auto& info : global_training_info_) {
      CHECK(info.has_nnz_per_row());
      CHECK(info.has_id());
      float b = std::round(
          std::max((float)1.0, info.nnz_per_row() * cf.feature_block_ratio()));
      int n = std::max((int)b, 1);
      for (int i = 0; i < n; ++i) {
        auto block = Range<Key>(info.col()).evenDivide(n, i);
        if (block.empty()) continue;
        blocks.push_back(std::make_pair(info.id(), block));
      }
    }
  }
  fprintf(stderr, "features are partitioned into %lu blocks\n", blocks.size());
  return blocks;
}


  // test fault tolarance
  //   Task recover;
  //   recover.mutable_risk()->set_cmd(CallRiskMin::RECOVER);
  //   App::testFaultTolerance(recover);

void LinearBlockIterator::prepareData(const Message& msg) {
  int time = msg.task.time() * 10;
  if (exec_.isWorker()) {
    // LL << myNodeID() << " training data " << app_cf_.training().DebugString();
    auto training_data = readMatrices<double>(app_cf_.training());
    CHECK_EQ(training_data.size(), 2);
    // LL << myNodeID() << " load training data";
    y_ = training_data[0];
    X_ = training_data[1]->localize(&(w_->key()));
    // LL << myNodeID() << " convert global fea_id into local ones";
    CHECK_EQ(y_->rows(), X_->rows());
    if (app_cf_.block_iterator().feature_block_ratio() > 0) {
      X_ = X_->toColMajor();
      // LL << myNodeID() << " to col major";
    }
    // sync keys and fetch initial value of w_
    SArrayList<double> empty;
    std::promise<void> promise;
    w_->roundTripForWorker(time, w_->key().range(), empty, [this, &promise](int t) {
        auto data = w_->received(t);
        CHECK_EQ(data.size(), 1);
        CHECK_EQ(w_->key().size(), data[0].first.size());
        w_->value() = data[0].second;
        promise.set_value();
      });
    promise.get_future().wait();
    // LL << myNodeID() << " received w";
    dual_.resize(X_->rows());
    dual_.eigenVector() = *X_ * w_->value().eigenVector();
  } else {
    w_->roundTripForServer(time, Range<Key>::all(), [this](int t){
        // LL << myNodeID() << " received keys";
        // init w by 0
        w_->value().resize(w_->key().size());
        auto init = app_cf_.init_w();
        if (init.type() == ParameterInitConfig::ZERO) {
          w_->value().setZero();
        } else if (init.type() == ParameterInitConfig::RANDOM) {
          w_->value().eigenVector() =
              Eigen::VectorXd::Random(w_->value().size()) * init.random_std();
          LL << w_->value().eigenVector().squaredNorm();
        } else {
          LL << "TOOD";
        }
      });
  }
}

RiskMinProgress LinearBlockIterator::evaluateProgress() {
  RiskMinProgress prog;
  if (exec_.isWorker()) {
    prog.set_objv(loss_->evaluate({y_, dual_.matrix()}));
    prog.add_busy_time(busy_timer_.get());
  } else {
    if (penalty_) prog.set_objv(penalty_->evaluate(w_->value().matrix()));
    prog.set_nnz_w(w_->nnz());
  }
  // LL << myNodeID() << ": objv " << prog.objv();
  return prog;
}

void LinearBlockIterator::updateModel(Message* msg) {

  auto time = msg->task.time() * 10;
  Range<Key> global_range(msg->task.risk().key());
  auto local_range = w_->localRange(global_range);

  if (exec_.isWorker()) {
    // CHECK(!local_range.empty());
    // if (local_range.empty()) LL << global_range << " " << local_range;
    // LL << global_range;
    // int id = msg->task.risk().feature_group_id();
    auto X = X_->colBlock(local_range);

    SArrayList<double> local_grads(2);
    local_grads[0].resize(local_range.size());
    local_grads[1].resize(local_range.size());
    AggGradLearnerArg arg;
    {
      Lock l(mu_);
      busy_timer_.start();
      learner_->compute({y_, X, dual_.matrix()}, arg, local_grads);
      busy_timer_.stop();
    }

    msg->finished = false;
    auto d = *msg;
    w_->roundTripForWorker(time, global_range, local_grads, [this, X, local_range, d] (int time) {
        Lock l(mu_);
        busy_timer_.start();

        if (!local_range.empty()) {
          auto data = w_->received(time);

          CHECK_EQ(data.size(), 1);
          CHECK_EQ(local_range, data[0].first);
          auto new_w = data[0].second;

          auto delta = new_w.eigenVector() - w_->segment(local_range).eigenVector();
          dual_.eigenVector() += *X * delta;
          w_->segment(local_range).eigenVector() = new_w.eigenVector();
        }

        busy_timer_.stop();
        taskpool(d.sender)->finishIncomingTask(d.task.time());
        sys_.reply(d);
        // LL << myNodeID() << " done " << d.task.time();
      });
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




} // namespace PS
