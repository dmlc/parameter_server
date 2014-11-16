#include "linear_method/batch_solver.h"

#include "util/split.h"
#include "base/matrix_io_inl.h"
#include "base/localizer.h"
#include "base/sparse_matrix.h"
#include "data/common.h"

namespace PS {

DECLARE_bool(verbose);

namespace LM {

void BatchSolver::init() {
  LinearMethod::init();
  if (IamWorker()) {
    worker_ = std::shared_ptr<BatchWorker>(new BatchWorker());
    worker_->init(app_cf_.parameter_name(0), conf_, this);
  } else if (IamServer()) {
    server_ = std::shared_ptr<BatchServer>(new BatchServer());
    server_->init(app_cf_.parameter_name(0), conf_);
  }
}

void BatchSolver::run() {
  // start the system
  LinearMethod::startSystem();

  // load data
  auto active_nodes = taskpool(kActiveGroup);
  auto load_time = tic();
  Task load = newTask(Call::LOAD_DATA);
  int hit_cache = 0;
  active_nodes->submitAndWait(load, [this, &hit_cache](){
      DataInfo info; CHECK(info.ParseFromString(exec_.lastRecvReply()));
      // LL << info.DebugString();
      g_train_info_ = mergeExampleInfo(g_train_info_, info.example_info());
      hit_cache += info.hit_cache();
    });
  if (hit_cache > 0) {
    CHECK_EQ(hit_cache, FLAGS_num_workers) << "clear the local caches";
    LI << "Hit local caches for the training data";
  }
  LI << "Loaded " << g_train_info_.num_ex() << " examples in "
     << toc(load_time) << " sec";

  // partition feature blocks
  CHECK(conf_.has_solver());
  auto sol_cf = conf_.solver();
  for (int i = 0; i < g_train_info_.slot_size(); ++i) {
    auto info = g_train_info_.slot(i);
    CHECK(info.has_id());
    if (info.id() == 0) continue;  // it's the label
    CHECK(info.has_nnz_ele());
    CHECK(info.has_nnz_ex());
    fea_grp_.push_back(info.id());
    double nnz_per_row = (double)info.nnz_ele() / (double)info.nnz_ex();
    int n = 1;  // number of blocks for a feature group
    if (nnz_per_row > 1 + 1e-6) {
      // only parititon feature group whose features are correlated
      n = std::max((int)std::ceil(nnz_per_row * sol_cf.feature_block_ratio()), 1);
    }
    for (int i = 0; i < n; ++i) {
      auto block = Range<Key>(info.min_key(), info.max_key()).evenDivide(n, i);
      if (block.empty()) continue;
      fea_blk_.push_back(std::make_pair(info.id(), block));
    }
  }

  // preprocess the training data
  auto preprocess_time = tic();
  Task preprocess = newTask(Call::PREPROCESS_DATA);
  for (auto grp : fea_grp_) set(&preprocess)->add_fea_grp(grp);
  set(&preprocess)->set_hit_cache(hit_cache > 0);
  active_nodes->submitAndWait(preprocess);
  if (sol_cf.tail_feature_freq()) {
    LI << "Features with frequency <= " << sol_cf.tail_feature_freq() << " are filtered";
  }
  LI << "Preprocessing is finished in " << toc(preprocess_time) << " sec";
  LI << "Features are partitioned into " << fea_blk_.size() << " blocks";

  // a simple block order
  for (int i = 0; i < fea_blk_.size(); ++i) blk_order_.push_back(i);

  // blocks for important feature groups
  std::vector<string> hit_blk;
  for (int i = 0; i < sol_cf.prior_fea_group_size(); ++i) {
    int grp_id = sol_cf.prior_fea_group(i);
    std::vector<int> tmp;
    for (int k = 0; k < fea_blk_.size(); ++k) {
      if (fea_blk_[k].first == grp_id) tmp.push_back(k);
    }
    if (tmp.empty()) continue;
    hit_blk.push_back(std::to_string(grp_id));
    for (int j = 0; j < sol_cf.num_iter_for_prior_fea_group(); ++j) {
      if (sol_cf.random_feature_block_order()) {
        std::random_shuffle(tmp.begin(), tmp.end());
      }
      prior_blk_order_.insert(prior_blk_order_.end(), tmp.begin(), tmp.end());
    }
  }
  if (!hit_blk.empty()) LI << "Prior feature groups: " + join(hit_blk, ", ");


  total_timer_.restart();
  runIteration();

  if (conf_.has_validation_data()) {
    // LI << "\tEvaluate with " << g_validation_info_[0].row().end()
    //    << " validation examples\n";
    Task test = newTask(Call::COMPUTE_VALIDATION_AUC);
    AUC validation_auc;
    active_nodes->submitAndWait(test, [this, &validation_auc](){
        mergeAUC(&validation_auc); });
    LI << "\tEvaluation accuracy: " << validation_auc.accuracy(0)
       << ", auc: " << validation_auc.evaluate();
  }

  Task save_model = newTask(Call::SAVE_MODEL);
  active_nodes->submitAndWait(save_model);
}

void BatchSolver::runIteration() {
  auto sol_cf = conf_.solver();
  auto pool = taskpool(kActiveGroup);
  int time = pool->time();
  int tau = sol_cf.max_block_delay();
  for (int iter = 0; iter < sol_cf.max_pass_of_data(); ++iter) {
    if (sol_cf.random_feature_block_order())
      std::random_shuffle(blk_order_.begin(), blk_order_.end());

    for (int b : blk_order_)  {
      Task update = newTask(Call::UPDATE_MODEL);
      update.set_wait_time(time - tau);
      // set the feature key range will be updated in this block
      fea_blk_[b].second.to(set(&update)->mutable_key());
      time = pool->submit(update);
    }

    Task eval = newTask(Call::EVALUATE_PROGRESS);
    eval.set_wait_time(time - tau);
    time = pool->submitAndWait(
        eval, [this, iter](){ LinearMethod::mergeProgress(iter); });

    showProgress(iter);

    double rel = g_progress_[iter].relative_objv();
    if (rel > 0 && rel <= sol_cf.epsilon()) {
      LI << "\tStopped: relative objective <= " << sol_cf.epsilon();
      break;
    }
  }
}


Progress BatchSolver::evaluateProgress() {
  Progress prog;
  // if (IamWorker()) {
  //   mu_.lock();
  //   busy_timer_.start();
  //   prog.set_objv(loss_->evaluate({y_, dual_.matrix()}));
  //   prog.add_busy_time(busy_timer_.get());
  //   busy_timer_.reset();
  //   mu_.unlock();
  // } else if (IamServer()) {
  //   if (penalty_) prog.set_objv(penalty_->evaluate(w_->value().matrix()));
  //   prog.set_nnz_w(w_->nnz());
  // }
  // // LL << myNodeID() << ": objv " << prog.objv();
  return prog;
}

void BatchSolver::showProgress(int iter) {
  int s = iter == 0 ? -3 : iter;
  for (int i = s; i <= iter; ++i) {
    showObjective(i);
    showNNZ(i);
    showTime(i);
  }
}

void BatchSolver::computeEvaluationAUC(AUCData *data) {
  if (!IamWorker()) return;

  // TODO
  // load data
  // CHECK(XXXX.has_validation_data());
  // if (!loadCache("valid")) {
  //   auto list = readMatricesOrDie<double>(XXXX.training_data());
  //   CHECK_EQ(list.size(), 2);
  //   y_ = list[0];
  //   auto X = std::static_pointer_cast<SparseMatrix<Key, double>>(list[1]);
  //   X->countUniqIndex(&w_->key());
  //   X_ = X->remapIndex(w_->key());
  //   saveCache("valid");
  // }

  // // fetch the model
  // MessagePtr pull_msg(new Message(kServerGroup, Message::kInvalidTime));
  // pull_msg->key = w_->key();
  // pull_msg->wait = true;
  // int time = w_->pull(pull_msg);
  // w_->value() = w_->received(time)[0].second;
  // CHECK_EQ(w_->key().size(), w_->value().size());

  // // w_->fetchValueFromServers();

  // // compute auc
  // AUC auc; auc.setGoodness(XXXX.block_solver().auc_goodness());
  // SArray<double> Xw(X_->rows());
  // for (auto& v : w_->value()) if (v != v) v = 0;
  // Xw.eigenVector() = *X_ * w_->value().eigenVector();
  // auc.compute(y_->value(), Xw, data);

  // debug
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


void BatchSolver::updateModel(const MessagePtr& msg) {
}

} // namespace LM
} // namespace PS
