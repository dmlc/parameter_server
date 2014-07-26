#include "linear_method/batch_solver.h"
#include "base/matrix_io.h"
#include "base/sparse_matrix.h"

namespace PS {
namespace LM {

void BatchSolver::init() {
  w_ = KVVectorPtr(new KVVector<Key, double>());
  w_->name() = app_cf_.parameter_name(0);
  sys_.yp().add(std::static_pointer_cast<Customer>(w_));
}

void BatchSolver::run() {
  // LinearMethod::startSystem();
  // auto blocks = BlockSolver::partitionFeatures(app_cf_, global_training_info_);
  // std::vector<int> block_order;
  // for (int i = 0; i < blocks.size(); ++i) block_order.push_back(i);
  // auto cf = app_cf_.block_iterator();

  // // iterating
  // auto pool = taskpool(kActiveGroup);
  // int time = pool->time();
  // int tau = cf.max_block_delay();
  // for (int iter = 0; iter < cf.max_pass_of_data(); ++iter) {
  //   if (cf.random_feature_block_order())
  //     std::random_shuffle(block_order.begin(), block_order.end());

  //   for (int b : block_order)  {
  //     Task update;
  //     update.set_wait_time(time - tau);
  //     auto cmd = RiskMinimization::setCall(&update);
  //     cmd->set_cmd(RiskMinCall::UPDATE_MODEL);
  //     // set the feature key range will be updated in this block
  //     blocks[b].second.to(cmd->mutable_key());
  //     time = pool->submit(update);
  //   }

  //   Task eval;
  //   RiskMinimization::setCall(&eval)->set_cmd(RiskMinCall::EVALUATE_PROGRESS);
  //   eval.set_wait_time(time - tau);

  //   time = pool->submit(eval, [this, iter](){ RiskMinimization::mergeProgress(iter); });
  //   pool->waitOutgoingTask(time);

  //   showProgress(iter);
  //   if (fabs(global_progress_[iter].relative_objv()) <= cf.epsilon()) {
  //     fprintf(stderr, "convergence criteria satisfied: relative objective <= %.1e\n", cf.epsilon());
  //     break;
  //   }
  // }

  // Task save_model;
  // RiskMinimization::setCall(&save_model)->set_cmd(RiskMinCall::SAVE_MODEL);
  // time = pool->submit(save_model);
  // pool->waitOutgoingTask(time);
}

void BatchSolver::prepareData(const Message& msg) {
  int time = msg.task.time() * 10;
  if (exec_.isWorker()) {
    auto training_data = readMatrices<double>(app_cf_.training_data());
    CHECK_EQ(training_data.size(), 2);
    y_ = training_data[0];
    X_ = training_data[1]->localize(&(w_->key()));
    CHECK_EQ(y_->rows(), X_->rows());
    if (app_cf_.block_solver().feature_block_ratio() > 0) {
      X_ = X_->toColMajor();
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


void BatchSolver::updateModel(Message* msg) {

  // auto time = msg->task.time() * 10;
  // Range<Key> global_range(msg->task.risk().key());
  // auto local_range = w_->localRange(global_range);

  // if (exec_.isWorker()) {
  //   // CHECK(!local_range.empty());
  //   // if (local_range.empty()) LL << global_range << " " << local_range;
  //   // LL << global_range;
  //   // int id = msg->task.risk().feature_group_id();
  //   auto X = X_->colBlock(local_range);

  //   SArrayList<double> local_grads(2);
  //   local_grads[0].resize(local_range.size());
  //   local_grads[1].resize(local_range.size());
  //   AggGradLearnerArg arg;
  //   {
  //     Lock l(mu_);
  //     busy_timer_.start();
  //     learner_->compute({y_, X, dual_.matrix()}, arg, local_grads);
  //     busy_timer_.stop();
  //   }

  //   msg->finished = false;
  //   auto d = *msg;
  //   w_->roundTripForWorker(time, global_range, local_grads, [this, X, local_range, d] (int time) {
  //       Lock l(mu_);
  //       busy_timer_.start();

  //       if (!local_range.empty()) {
  //         auto data = w_->received(time);

  //         CHECK_EQ(data.size(), 1);
  //         CHECK_EQ(local_range, data[0].first);
  //         auto new_w = data[0].second;

  //         auto delta = new_w.eigenVector() - w_->segment(local_range).eigenVector();
  //         dual_.eigenVector() += *X * delta;
  //         w_->segment(local_range).eigenVector() = new_w.eigenVector();
  //       }

  //       busy_timer_.stop();
  //       taskpool(d.sender)->finishIncomingTask(d.task.time());
  //       sys_.reply(d);
  //       // LL << myNodeID() << " done " << d.task.time();
  //     });
  // } else {
  //   // aggregate local gradients, then update model
  //   w_->roundTripForServer(time, global_range, [this, local_range] (int time) {
  //       SArrayList<double> aggregated_gradient;
  //       for (auto& d : w_->received(time)) {
  //         CHECK_EQ(local_range, d.first);
  //         aggregated_gradient.push_back(d.second);
  //       }
  //       AggGradLearnerArg arg;
  //       arg.set_learning_rate(app_cf_.learner().learning_rate());
  //       learner_->update(aggregated_gradient, arg, w_->segment(local_range));
  //     });
  // }

}

RiskMinProgress BatchSolver::evaluateProgress() {
  RiskMinProgress prog;
  if (exec_.isWorker()) {
    prog.set_objv(loss_->evaluate({y_, dual_.matrix()}));
    prog.add_busy_time(busy_timer_.get());
    busy_timer_.reset();
  } else {
    if (penalty_) prog.set_objv(penalty_->evaluate(w_->value().matrix()));
    prog.set_nnz_w(w_->nnz());
  }
  // LL << myNodeID() << ": objv " << prog.objv();
  return prog;
}

void BatchSolver::saveModel(const Message& msg) {
  // didn't use msg here. in future, one may pass the model_file by msg

  if (!exec_.isServer()) return;
  if (!app_cf_.has_model_output()) return;

  auto output = app_cf_.model_output();
  // if (output.files_size() != 1) {
  //   LL << "you should use only a single file: " << output.DebugString();
  //   return;
  // }

  CHECK_EQ(w_->key().size(), w_->value().size());

  if (output.format() == DataConfig::TEXT) {
    std::string file = w_->name() + "_" + exec_.myNode().id();
    if (output.files_size() > 0) file = output.files(0) + file;
    fprintf(stderr, "%s writes model to %s\n",
            exec_.myNode().id().data(), file.data());
    std::ofstream out(file);
    CHECK(out.good());
    for (size_t i = 0; i < w_->key().size(); ++i) {
      auto v = w_->value()[i];
      if (v != 0 && !(v != v)) out << w_->key()[i] << "\t" << v << "\n";
    }
  } else {
    LL << "didn't implement yet";
  }
}

void BatchSolver::showProgress(int iter) {
  int s = iter == 0 ? -3 : iter;
  for (int i = s; i <= iter; ++i) {
    RiskMinimization::showObjective(i);
    RiskMinimization::showNNZ(i);
    RiskMinimization::showTime(i);
  }
}

void BatchSolver::computeEvaluationAUC(AUCData *data) {
  if (!exec_.isWorker()) return;
  CHECK(app_cf_.has_validation_data());
  auto validation_data = readMatrices<double>(app_cf_.validation_data());
  CHECK_EQ(validation_data.size(), 2);

  auto y = validation_data[0]->value();
  auto X = validation_data[1]->localize(&(w_->key()));
  CHECK_EQ(y.size(), X->rows());

  w_->fetchValueFromServers();

  // w.writeToFile("w");
  // CHECK_EQ.size(), w.size());

  AUC auc; auc.setGoodness(app_cf_.block_solver().auc_goodness());
  SArray<double> Xw(X->rows());
  for (auto& v : w_->value()) if (v != v) v = 0;
  Xw.eigenVector() = *X * w_->value().eigenVector();
  auc.compute(y, Xw, data);

  // Xw.writeToFile("Xw_"+myNodeID());
  // y.writeToFile("y_"+myNodeID());
  // LL << auc.evaluate();
}

} // namespace LM
} // namespace PS
