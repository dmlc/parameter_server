#include "linear_method/darlin.h"
#include "base/matrix_io.h"
#include "base/sparse_matrix.h"
namespace PS {

DECLARE_bool(verbose);

namespace LM {

void Darlin::runIteration() {
  CHECK(conf_.has_darling());
  CHECK_EQ(conf_.loss().type(), LossConfig::LOGIT);
  CHECK_EQ(conf_.penalty().type(), PenaltyConfig::L1);
  auto sol_cf = conf_.solver();
  int tau = sol_cf.max_block_delay();
  kkt_filter_threshold_ = 1e20;
  bool reset_kkt_filter = false;
  bool random_blk_order = sol_cf.random_feature_block_order();
  if (!random_blk_order) {
    LI << "Warning: Randomized block order often acclerates the convergence.";
  }
  LI << "Train l_1 logistic regression by " << tau << "-delayed block coordinate descent";

  // iterating
  int max_iter = sol_cf.max_pass_of_data();
  auto pool = taskpool(kActiveGroup);
  int time = pool->time() + fea_grp_.size() * 2;
  const int first_update_model_task_id = time + 1;
  for (int iter = 0; iter < max_iter; ++iter) {
    // pick up a updating order
    auto order = blk_order_;
    if (random_blk_order) std::random_shuffle(order.begin(), order.end());
    if (iter == 0) order.insert(
            order.begin(), prior_blk_order_.begin(), prior_blk_order_.end());

    // iterating on feature blocks
    for (int i = 0; i < order.size(); ++i) {
      Task update = newTask(Call::UPDATE_MODEL);
      update.set_time(time+1);
      if (iter == 0 && i < prior_blk_order_.size()) {
        // force zero delay for important feature blocks
        update.set_wait_time(time);
      } else {
        update.set_wait_time(time - tau);
      }

      // make sure leading UPDATE_MODEL tasks could be picked up by workers
      if (update.time() - tau <= first_update_model_task_id) {
        update.set_wait_time(-1);
      }

      auto cmd = set(&update);
      if (i == 0) {
        cmd->set_kkt_filter_threshold(kkt_filter_threshold_);
        if (reset_kkt_filter) cmd->set_reset_kkt_filter(true);
      }
      auto blk = fea_blk_[order[i]];
      blk.second.to(cmd->mutable_key());
      cmd->add_fea_grp(blk.first);
      time = pool->submit(update);
    }

    // evaluate the progress
    Task eval = newTask(Call::EVALUATE_PROGRESS);
    eval.set_wait_time(time - tau);
    time = pool->submitAndWait(
        eval, [this, iter](){ LinearMethod::mergeProgress(iter); });
    showProgress(iter);

    // update the kkt filter strategy
    double vio = g_progress_[iter].violation();
    double ratio = conf_.darling().kkt_filter_threshold_ratio();
    kkt_filter_threshold_ = vio / (double)g_train_info_.num_ex() * ratio;

    // check if finished
    double rel = g_progress_[iter].relative_objv();
    if (rel > 0 && rel <= sol_cf.epsilon()) {
      if (reset_kkt_filter) {
        LI << "Stopped: relative objective <= " << sol_cf.epsilon();
        break;
      } else {
        reset_kkt_filter = true;
      }
    } else {
      reset_kkt_filter = false;
    }
    if (iter == max_iter - 1) {
      LI << "Reached maximal " << max_iter << " data passes";
    }
  }
}

void Darlin::preprocessData(const MessageCPtr& msg) {
  auto time = msg->task.time() * kPace;
  if (IamWorker()) {
    worker_->preprocessData(time, get(msg));
  } else if (IamServer()) {
    server_->preprocessData(time, get(msg));
  }
}

void Darlin::updateModel(const MessagePtr& msg) {
  if (FLAGS_verbose) {
    LI << "updateModel; msg [" << msg->shortDebugString() << "]";
  }
  auto time = msg->task.time() * kPace;
  if (IamWorker()) {
    worker_->computeGradient(time, msg);
  } else if (IamServer()) {
    server_->updateWeight(time, get(msg));
  }
}

void Darlin::showKKTFilter(int iter) {
  if (iter == -3) {
    fprintf(stderr, "|      KKT filter     ");
  } else if (iter == -2) {
    fprintf(stderr, "| threshold  #activet ");
  } else if (iter == -1) {
    fprintf(stderr, "+---------------------");
  } else {
    auto prog = g_progress_[iter];
    fprintf(stderr, "| %.1e %11llu ", kkt_filter_threshold_, (uint64)prog.nnz_active_set());
  }
}

void Darlin::showProgress(int iter) {
  int s = iter == 0 ? -3 : iter;
  for (int i = s; i <= iter; ++i) {
    showObjective(i);
    showNNZ(i);
    showKKTFilter(i);
    showTime(i);
  }
}

} // namespace LM
} // namespace PS
