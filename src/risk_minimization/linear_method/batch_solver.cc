#include "risk_minimization/linear_method/batch_solver.h"
#include "base/matrix_io_inl.h"
#include "base/sparse_matrix.h"

namespace PS {
namespace LM {

void BatchSolver::init() {
  w_ = KVVectorPtr(new KVVector<Key, double>());
  w_->name() = app_cf_.parameter_name(0);
  sys_.yp().add(std::static_pointer_cast<Customer>(w_));
}


void BatchSolver::run() {
  LinearMethod::startSystem();

  // evenly partition feature blocks
  CHECK(app_cf_.has_block_solver());
  auto cf = app_cf_.block_solver();
  if (cf.feature_block_ratio() <= 0) {
    Range<Key> range(-1, 0);
    for (const auto& info : g_training_info_)
      range.setUnion(Range<Key>(info.col()));
    fea_blocks_.push_back(std::make_pair(-1, range));
  } else {
    for (const auto& info : g_training_info_) {
      CHECK(info.has_nnz_per_row());
      CHECK(info.has_id());
      float b = std::round(
          std::max((float)1.0, info.nnz_per_row() * cf.feature_block_ratio()));
      int n = std::max((int)b, 1);
      for (int i = 0; i < n; ++i) {
        auto block = Range<Key>(info.col()).evenDivide(n, i);
        if (block.empty()) continue;
        fea_blocks_.push_back(std::make_pair(info.id(), block));
      }
    }
  }
  LI << "features are partitioned into " << fea_blocks_.size() << " blocks",

  // a simple block order
  block_order_.clear();
  for (int i = 0; i < fea_blocks_.size(); ++i) block_order_.push_back(i);

  // load data
  Task prepare;
  prepare.set_type(Task::CALL_CUSTOMER);
  prepare.mutable_risk()->set_cmd(RiskMinCall::PREPARE_DATA);
  taskpool(kActiveGroup)->submitAndWait(prepare);
  init_sys_time_ = total_timer_.get();
  LI << "loaded data... in " << init_sys_time_ << " sec";

  runIteration();

  auto active = taskpool(kActiveGroup);

  // Task save_dense_data = newTask(RiskMinCall::SAVE_AS_DENSE);
  // auto mut_data = setCall(&save_dense_data);
  // mut_data->set_name(name()+"_train");
  // for (const auto& info : global_training_info_) {
  //   Range<Key>(info.col()).to(mut_data->add_reduce_range());
  // }
  // active->submitAndWait(save_dense_data);


  if (app_cf_.has_validation_data()) {
    LI << "evaluate with " << g_validation_info_[0].row().end()
       << " validation examples\n";
    Task test = newTask(RiskMinCall::COMPUTE_VALIDATION_AUC);
    AUC validation_auc;
    active->submitAndWait(test, [this, &validation_auc](){
        mergeAUC(&validation_auc); });
    LI << "evaluation accuracy: " << validation_auc.accuracy(0)
       << ", auc: " << validation_auc.evaluate();

    // auto active = taskpool(kActiveGroup);
    // Task save_dense_data = newTask(RiskMinCall::SAVE_AS_DENSE);
    // auto mut_data = setCall(&save_dense_data);
    // mut_data->set_name(name()+"_test");
    // for (const auto& info : global_training_info_) {
    //   Range<Key>(info.col()).to(mut_data->add_reduce_range());
    // }
    // active->submitAndWait(save_dense_data);
  }

  Task save_model = newTask(RiskMinCall::SAVE_MODEL);
  active->submitAndWait(save_model);
}

void BatchSolver::runIteration() {
  auto cf = app_cf_.block_solver();
  auto pool = taskpool(kActiveGroup);
  int time = pool->time();
  int tau = cf.max_block_delay();
  for (int iter = 0; iter < cf.max_pass_of_data(); ++iter) {
    if (cf.random_feature_block_order())
      std::random_shuffle(block_order_.begin(), block_order_.end());

    for (int b : block_order_)  {
      Task update = newTask(RiskMinCall::UPDATE_MODEL);
      update.set_wait_time(time - tau);
      // set the feature key range will be updated in this block
      fea_blocks_[b].second.to(setCall(&update)->mutable_key());
      time = pool->submit(update);
    }

    Task eval = newTask(RiskMinCall::EVALUATE_PROGRESS);
    eval.set_wait_time(time - tau);
    time = pool->submitAndWait(
        eval, [this, iter](){ RiskMinimization::mergeProgress(iter); });

    showProgress(iter);

    double rel = global_progress_[iter].relative_objv();
    if (rel > 0 && rel <= cf.epsilon()) {
      LI << "stopped: relative objective <= " << cf.epsilon();
      break;
    }
  }
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
              Eigen::VectorXd::Random(w_->value().size()) * init.std();
          LL << w_->value().eigenVector().squaredNorm();
        } else {
          LL << "TOOD";
        }
      });
  }
}


void BatchSolver::updateModel(Message* msg) {
  int time = msg->task.time() * 10;
  Range<Key> global_range(msg->task.risk().key());
  auto local_range = w_->localRange(global_range);

  if (exec_.isWorker()) {
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
        arg.set_learning_rate(app_cf_.learning_rate().eta());
        learner_->update(aggregated_gradient, arg, w_->segment(local_range));
      });
  }

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
  if (!exec_.isServer()) return;
  if (!app_cf_.has_model_output()) return;

  auto output = app_cf_.model_output();
  // if (output.files_size() != 1) {
  //   LL << "you should use only a single file: " << output.DebugString();
  //   return;
  // }

  CHECK_EQ(w_->key().size(), w_->value().size());

  if (output.format() == DataConfig::TEXT) {
    // TODO use the model_file in msg
    std::string file = "../output/" + w_->name() + "_" + exec_.myNode().id();
    if (output.file_size() > 0) file = output.file(0) + file;
    LI << exec_.myNode().id() << " writes model to " << file;
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

  y_ = validation_data[0];
  X_ = validation_data[1]->localize(&(w_->key()));
  CHECK_EQ(y_->rows(), X_->rows());

  w_->fetchValueFromServers();

  // w.writeToFile("w");

  AUC auc; auc.setGoodness(app_cf_.block_solver().auc_goodness());
  SArray<double> Xw(X_->rows());
  for (auto& v : w_->value()) if (v != v) v = 0;
  Xw.eigenVector() = *X_ * w_->value().eigenVector();
  auc.compute(y_->value(), Xw, data);

  // double correct = 0;
  // for (int i = 0; i < Xw.size(); ++i)
  //   if (y_->value()[i] * Xw[i] >= 0) correct += 1;
  // LL << correct / Xw.size();

  // Xw.writeToFile("Xw_"+myNodeID());
  // y_->value().writeToFile("y_"+myNodeID());
  // LL << auc.evaluate();
}

void BatchSolver::saveAsDenseData(const Message& msg) {
  if (!exec_.isWorker()) return;
  auto call = RiskMinimization::getCall(msg);
  int n = call.reduce_range_size();
  if (n == 0) return;
  if (X_->rowMajor()) {
    X_ = X_->toColMajor();
  }
  DenseMatrix<double> Xw(X_->rows(), n, false);
  for (int i = 0; i < n; ++i) {
    auto lr = w_->localRange(Range<Key>(call.reduce_range(i)));
    Xw.colBlock(SizeR(i, i+1))->eigenArray() =
        *(X_->colBlock(lr)) * w_->segment(lr).eigenVector();
  }

  Xw.writeToBinFile(call.name()+"_Xw");
  y_->writeToBinFile(call.name()+"_y");
}



} // namespace LM
} // namespace PS
