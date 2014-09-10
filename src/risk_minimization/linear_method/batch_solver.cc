#include "risk_minimization/linear_method/batch_solver.h"
#include "util/split.h"
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
  // start the system
  LinearMethod::startSystem();

  // load data
  timer_.start();
  Task prepare;
  prepare.set_type(Task::CALL_CUSTOMER);
  prepare.mutable_risk()->set_cmd(RiskMinCall::PREPARE_DATA);
  taskpool(kActiveGroup)->submitAndWait(prepare, [this](){
      InstanceInfo info;
      CHECK(info.ParseFromString(exec_.lastRecvReply()));
      // LL << info.DebugString();
      g_train_ins_info_ = mergeInstanceInfo(g_train_ins_info_, info);
    });
  LI << "\tLoaded " << g_train_ins_info_.num_ins() << " training instances... in "
     << timer_.stop() << " sec";

  // partition feature blocks
  CHECK(app_cf_.has_block_solver());
  auto cf = app_cf_.block_solver();
  for (int i = 1; i < g_train_ins_info_.fea_group_size(); ++i) {
    auto info = g_train_ins_info_.fea_group(i);
    CHECK(info.has_nnz());
    CHECK(info.has_num_nonempty_ins());
    double nnz_per_row = (double)info.nnz() / (double)info.num_nonempty_ins();
    int n = 1;
    if (nnz_per_row > 1 + 1e-6) {
      n = std::max((int)std::ceil(nnz_per_row*cf.feature_block_ratio()), 1);
    }
    for (int i = 0; i < n; ++i) {
      auto block = Range<Key>(info.fea_begin(), info.fea_end()).evenDivide(n, i);
      if (block.empty()) continue;
      fea_blocks_.push_back(std::make_pair(info.group_id(), block));
    }
  }
  LI << "\tFeatures are partitioned into " << fea_blocks_.size() << " blocks";

  // a simple block order
  for (int i = 0; i < fea_blocks_.size(); ++i) block_order_.push_back(i);

  // blocks of important features
  std::vector<string> hit_blk;
  for (int i = 0; i < cf.prior_fea_group_size(); ++i) {
    int group_id = cf.prior_fea_group(i);
    std::vector<int> tmp;
    for (int k = 0; k < fea_blocks_.size(); ++k) {
      if (fea_blocks_[k].first == group_id) tmp.push_back(k);
    }
    if (tmp.empty()) continue;
    hit_blk.push_back(std::to_string(group_id));
    for (int j = 0; j < cf.num_iter_for_prior_fea_group(); ++j) {
      if (cf.random_feature_block_order()) {
        std::random_shuffle(tmp.begin(), tmp.end());
      }
      prior_block_order_.insert(prior_block_order_.end(), tmp.begin(), tmp.end());
    }
  }
  if (!hit_blk.empty()) {
    LI << "\tFirst update feature group: " + join(hit_blk, ", ");
  }

  timer_.restart();
  runIteration();

  auto active = taskpool(kActiveGroup);
  if (app_cf_.has_validation_data()) {
    // LI << "\tEvaluate with " << g_validation_info_[0].row().end()
    //    << " validation examples\n";
    Task test = newTask(RiskMinCall::COMPUTE_VALIDATION_AUC);
    AUC validation_auc;
    active->submitAndWait(test, [this, &validation_auc](){
        mergeAUC(&validation_auc); });
    LI << "\tEvaluation accuracy: " << validation_auc.accuracy(0)
       << ", auc: " << validation_auc.evaluate();
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
      LI << "\tStopped: relative objective <= " << cf.epsilon();
      break;
    }
  }
}

bool BatchSolver::loadCache(const string& cache_name) {
  if (!app_cf_.has_local_cache()) return false;
  auto cache = app_cf_.local_cache();
  auto y_conf = ithFile(cache, 0, "_" + cache_name + "_y_" + myNodeID());
  auto X_conf = ithFile(cache, 0, "_" + cache_name + "_X_" + myNodeID());
  auto key_conf = ithFile(cache, 0, "_" + cache_name + "_key_" + myNodeID());
  MatrixPtrList<double> y_list, X_list;
  if (!(readMatrices<double>(y_conf, &y_list) &&
        readMatrices<double>(X_conf, &X_list) &&
        w_->key().readFromFile(SizeR(0, X_list[0]->cols()), key_conf))) {
    return false;
  }
  y_ = y_list[0];
  X_ = X_list[0];
  LI << "\t" << myNodeID() << " hit cache in " << cache.file(0) << " for " << cache_name;
  return true;
}

bool BatchSolver::saveCache(const string& cache_name) {
  if (!app_cf_.has_local_cache()) return false;
  auto cache = app_cf_.local_cache();
  auto y_conf = ithFile(cache, 0, "_" + cache_name + "_y_" + myNodeID());
  auto X_conf = ithFile(cache, 0, "_" + cache_name + "_X_" + myNodeID());
  auto key_conf = ithFile(cache, 0, "_" + cache_name + "_key_" + myNodeID());
  return (y_->writeToBinFile(y_conf.file(0)) &&
          X_->writeToBinFile(X_conf.file(0)) &&
          w_->key().writeToFile(key_conf.file(0)));
}


InstanceInfo BatchSolver::prepareData(const Message& msg) {
  int time = msg.task.time() * 10;
  if (exec_.isWorker()) {
    // load local training data
    bool hit_cache = loadCache("train");
    SArray<Key> uniq_key;
    SArray<uint32> key_cnt;
    SparseMatrixPtr<Key, double> X;
    if (!hit_cache) {
      auto list = readMatricesOrDie<double>(app_cf_.training_data());
      CHECK_EQ(list.size(), 2);
      y_ = list[0];
      X = std::static_pointer_cast<SparseMatrix<Key, double>>(list[1]);
      X->countUniqIndex(&uniq_key, &key_cnt);
    }

    // Time 0: send all unique keys with their count to servers
    MessagePtr count(new Message(kServerGroup, time));
    count->addKV(uniq_key, {key_cnt});
    w_->set(count)->set_add_key_count(true);
    CHECK_EQ(time, w_->push(count));

    // Time 2: filter tail features
    MessagePtr filter(new Message(kServerGroup, time+2, time+1));
    filter->key = uniq_key;
    w_->set(filter)->set_key_freq(app_cf_.block_solver().tail_feature_count());
    filter->wait = true;
    CHECK_EQ(time+2, w_->pull(filter));

    if (!hit_cache) {
      X_ = X->remapIndex(w_->key())->toColMajor();
      saveCache("train");
    }

    // Time 3: send filtered keys to servers
    MessagePtr push_key(new Message(kServerGroup, time+3));
    push_key->key = w_->key();
    w_->set(push_key)->set_add_key(true);
    CHECK_EQ(time+3, w_->push(push_key));

    // Time 5: fetch initial value of w_
    MessagePtr pull_val(new Message(kServerGroup, time+5, time+4));
    pull_val->key = w_->key();
    pull_val->wait = true;
    CHECK_EQ(time+5, w_->pull(pull_val));

    // set the value of w_
    auto init_w = w_->received(time+5);
    CHECK_EQ(init_w.size(), 1);
    CHECK_EQ(w_->key().size(), init_w[0].first.size());
    w_->value() = init_w[0].second;

    // set the local variable
    dual_.resize(X_->rows());
    dual_.eigenVector() = *X_ * w_->value().eigenVector();

    return y_->info().ins_info();
  } else {
    // Time 0: aggregate unfiltered keys from all workers
    w_->wait(kWorkerGroup, time);

    // Time 1: doing nothing
    w_->finish(kWorkerGroup, time+1);

    // Time 3: aggregate filtered keys from all workers
    w_->wait(kWorkerGroup, time+3);

    // Time 4: initial value of w_
    w_->value().resize(w_->key().size());
    w_->value().setValue(app_cf_.init_w());
    w_->finish(kWorkerGroup, time+4);
  }
  return InstanceInfo();
}



RiskMinProgress BatchSolver::evaluateProgress() {
  RiskMinProgress prog;
  if (exec_.isWorker()) {
    mu_.lock();
    busy_timer_.start();
    prog.set_objv(loss_->evaluate({y_, dual_.matrix()}));
    prog.add_busy_time(busy_timer_.get());
    busy_timer_.reset();
    mu_.unlock();
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
    LI << "\t" << exec_.myNode().id() << " writes model to " << file;
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

  // load data
  CHECK(app_cf_.has_validation_data());
  if (!loadCache("valid")) {
    auto list = readMatricesOrDie<double>(app_cf_.training_data());
    CHECK_EQ(list.size(), 2);
    y_ = list[0];
    auto X = std::static_pointer_cast<SparseMatrix<Key, double>>(list[1]);
    X->countUniqIndex(&w_->key());
    X_ = X->remapIndex(w_->key());
    saveCache("valid");
  }

  // fetch the model
  w_->fetchValueFromServers();

  // compute auc
  AUC auc; auc.setGoodness(app_cf_.block_solver().auc_goodness());
  SArray<double> Xw(X_->rows());
  for (auto& v : w_->value()) if (v != v) v = 0;
  Xw.eigenVector() = *X_ * w_->value().eigenVector();
  auc.compute(y_->value(), Xw, data);

  // w.writeToFile("w");
  // double correct = 0;
  // for (int i = 0; i < Xw.size(); ++i)
  //   if (y_->value()[i] * Xw[i] >= 0) correct += 1;
  // LL << correct / Xw.size();

  // Xw.writeToFile("Xw_"+myNodeID());
  // y_->value().writeToFile("y_"+myNodeID());
  // LL << auc.evaluate();
}

// void BatchSolver::saveAsDenseData(const Message& msg) {
//   if (!exec_.isWorker()) return;
//   auto call = RiskMinimization::getCall(msg);
//   int n = call.reduce_range_size();
//   if (n == 0) return;
//   if (X_->rowMajor()) {
//     X_ = X_->toColMajor();
//   }
//   DenseMatrix<double> Xw(X_->rows(), n, false);
//   for (int i = 0; i < n; ++i) {
//     auto lr = w_->localRange(Range<Key>(call.reduce_range(i)));
//     Xw.colBlock(SizeR(i, i+1))->eigenArray() =
//         *(X_->colBlock(lr)) * w_->segment(lr).eigenVector();
//   }

//   Xw.writeToBinFile(call.name()+"_Xw");
//   y_->writeToBinFile(call.name()+"_y");
// }


void BatchSolver::updateModel(Message* msg) {
  // FIXME several tiny bugs here...
  // int time = msg->task.time() * 10;
  // Range<Key> global_range(msg->task.risk().key());
  // auto local_range = w_->localRange(global_range);

  // if (exec_.isWorker()) {
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
  //   auto sender = msg->sender;
  //   auto task = msg->task;
  //   w_->roundTripForWorker(time, global_range, local_grads,
  //                          [this, X, local_range, sender, task] (int time) {
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

  //       taskpool(sender)->finishIncomingTask(task.time());
  //       sys_.reply(sender, task);
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
  //       arg.set_learning_rate(app_cf_.learning_rate().eta());
  //       learner_->update(aggregated_gradient, arg, w_->segment(local_range));
  //     });
  // }
}

} // namespace LM
} // namespace PS
