#include "linear_method/darling.h"
#include "base/matrix_io.h"
#include "base/sparse_matrix.h"

namespace PS {

DECLARE_bool(verbose);

namespace LM {

void Darling::init() {
  BatchSolver::init();
  // set is as a nan. the reason choosing kuint64max is because snappy has good
  // compression rate on 0xffff..ff
  memcpy(&kInactiveValue_, &kuint64max, sizeof(double));
}

void Darling::runIteration() {
  CHECK(conf_.has_darling());
  CHECK_EQ(conf_.loss().type(), LossConfig::LOGIT);
  CHECK_EQ(conf_.penalty().type(), PenaltyConfig::L1);
  auto sol_cf = conf_.solver();
  int tau = sol_cf.max_block_delay();
  KKT_filter_threshold_ = 1e20;
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
        cmd->set_kkt_filter_threshold(KKT_filter_threshold_);
        if (reset_kkt_filter) cmd->set_kkt_filter_reset(true);
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
    KKT_filter_threshold_ = vio / (double)g_train_info_.num_ex() * ratio;

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

void Darling::preprocessData(const MessageCPtr& msg) {
  BatchSolver::preprocessData(msg);
  if (IamWorker()) {
    // dual_ = exp(y.*(X_*w_))
    dual_.eigenArray() = exp(y_->value().eigenArray() * dual_.eigenArray());
  }
  for (int grp : fea_grp_) {
    size_t n = w_->key(grp).size();
    active_set_[grp].resize(n, true);
    delta_[grp].resize(n);
    delta_[grp].setValue(conf_.darling().delta_init_value());
  }

  // size_t mem = 0;
  // for (const auto& it : X_) mem += it.second->memSize();
  // for (const auto& it : active_set_) mem += it.second.memSize();
  // for (const auto& it : delta_) mem += it.second.memSize();
  // mem += dual_.memSize();
  // mem += w_->memSize();
  // LL << ResUsage::myPhyMem() << " " << mem / 1e6 ;


}

void Darling::updateModel(const MessagePtr& msg) {
  if (FLAGS_verbose) {
    LI << "updateModel; msg [" << msg->shortDebugString() << "]";
  }

  CHECK_GT(FLAGS_num_threads, 0);
  auto time = msg->task.time() * kPace;
  auto call = get(msg);
  if (call.has_kkt_filter_threshold()) {
    KKT_filter_threshold_ = call.kkt_filter_threshold();
    violation_ = 0;
  }
  if (call.has_kkt_filter_reset() && call.kkt_filter_reset()) {
    for (int grp : fea_grp_) active_set_[grp].fill(true);
  }
  CHECK_EQ(call.fea_grp_size(), 1);
  int grp = call.fea_grp(0);
  Range<Key> g_key_range(call.key());
  auto seg_pos = w_->find(grp, g_key_range);

  if (IamWorker()) {
    // compute local gradients
    mu_.lock();

    this->sys_.hb().startTimer(HeartbeatInfo::TimerType::BUSY);
    busy_timer_.start();
    auto local_gradients = computeGradients(grp, seg_pos);
    busy_timer_.stop();
    this->sys_.hb().stopTimer(HeartbeatInfo::TimerType::BUSY);

    mu_.unlock();

    // time 0: push local gradients
    MessagePtr push_msg(new Message(kServerGroup, time));
    auto local_keys = w_->key(grp).segment(seg_pos);
    push_msg->addKV(local_keys, local_gradients);
    g_key_range.to(push_msg->task.mutable_key_range());
    push_msg->task.set_key_channel(grp);
    CHECK_EQ(time, w_->push(push_msg));

    // time 1: servers do update, none of my business
    // time 2: pull the updated model from servers
    msg->finished = false; // not finished until model updates are pulled
    MessagePtr pull_msg(new Message(kServerGroup, time+2, time+1));
    pull_msg->key = local_keys;
    g_key_range.to(pull_msg->task.mutable_key_range());
    pull_msg->task.set_key_channel(grp);
    // the callback for updating the local dual variable
    pull_msg->fin_handle = [this, grp, seg_pos, time, msg] () {
      if (!seg_pos.empty()) {
        auto data = w_->received(time+2);
        CHECK_EQ(data.size(), 1); CHECK_EQ(seg_pos, data[0].first);
        mu_.lock();

        this->sys_.hb().startTimer(HeartbeatInfo::TimerType::BUSY);
        busy_timer_.start();
        updateDual(grp, seg_pos, data[0].second);
        busy_timer_.stop();
        this->sys_.hb().stopTimer(HeartbeatInfo::TimerType::BUSY);

        mu_.unlock();
      }
      // now finished, reply the scheduler
      taskpool(msg->sender)->finishIncomingTask(msg->task.time());
      sys_.reply(msg->sender, msg->task);
    };
    CHECK_EQ(time+2, w_->pull(pull_msg));
  } else if (IamServer()) {
    // none of my bussiness
    if (w_->myKeyRange().setIntersection(g_key_range).empty()) return;

    // time 0: aggregate all workers' local gradients
    w_->waitInMsg(kWorkerGroup, time);

    // time 1: update model
    if (!seg_pos.empty()) {
      auto data = w_->received(time);
      CHECK_EQ(data.size(), 2);
      CHECK_EQ(seg_pos, data[0].first);
      CHECK_EQ(seg_pos, data[1].first);

      this->sys_.hb().startTimer(HeartbeatInfo::TimerType::BUSY);
      updateWeight(grp, seg_pos, data[0].second, data[1].second);
      this->sys_.hb().stopTimer(HeartbeatInfo::TimerType::BUSY);
    }
    w_->finish(kWorkerGroup, time+1);

    // time 2: let the workers pull from me
  }
}

SArrayList<double> Darling::computeGradients(int grp, SizeR col_range) {
  SArrayList<double> grads(2);
  for (int i : {0, 1} ) {
    grads[i].resize(col_range.size());
    grads[i].setZero();
  }
  // TODO partition by rows for small col_range size
  int num_threads = col_range.size() < 64 ? 1 : FLAGS_num_threads;
  ThreadPool pool(num_threads);
  int npart = num_threads * 1;  // could use a larger partition number
  for (int i = 0; i < npart; ++i) {
    auto thr_range = col_range.evenDivide(npart, i);
    if (thr_range.empty()) continue;
    auto gr = thr_range - col_range.begin();
    pool.add([this, grp, thr_range, gr, &grads]() {
        computeGradients(grp, thr_range, grads[0].segment(gr), grads[1].segment(gr));
      });
  }
  pool.startWorkers();
  return grads;
}

void Darling::computeGradients(
    int grp, SizeR col_range, SArray<double> G, SArray<double> U) {
  CHECK_EQ(G.size(), col_range.size());
  CHECK_EQ(U.size(), col_range.size());
  CHECK(X_[grp]->colMajor());

  const auto& active_set = active_set_[grp];
  const auto& delta = delta_[grp];
  const double* y = y_->value().data();
  auto X = std::static_pointer_cast<SparseMatrix<uint32, double>>(
      X_[grp]->colBlock(col_range));
  const size_t* offset = X->offset().data();
  uint32* index = X->index().data() + offset[0];
  double* value = X->value().data() + offset[0];
  bool binary = X->binary();

  // j: column id, i: row id
  for (size_t j = 0; j < X->cols(); ++j) {
    size_t k = j + col_range.begin();
    size_t n = offset[j+1] - offset[j];
    if (!active_set.test(k)) {
      index += n;
      if (!binary) value += n;
      G[j] = U[j] = kInactiveValue_;
      continue;
    }
    double g = 0, u = 0;
    double d = binary ? exp(delta[k]) : delta[k];
    // TODO unroll loop
    for (size_t o = 0; o < n; ++o) {
      auto i = *(index ++);
      double tau = 1 / ( 1 + dual_[i] );
      if (binary) {
        g -= y[i] * tau;
        u += std::min(tau*(1-tau)*d, .25);
        // u += tau * (1-tau);
      } else {
        double v = *(value++);
        g -= y[i] * tau * v;
        u += std::min(tau*(1-tau)*exp(fabs(v)*d), .25) * v * v;
        // u += tau * (1-tau) * v * v;
      }
    }
    G[j] = g; U[j] = u;
  }
}

void Darling::updateDual(int grp, SizeR col_range, SArray<double> new_w) {
  SArray<double> delta_w(new_w.size());
  auto& cur_w = w_->value(grp);
  auto& active_set = active_set_[grp];
  auto& delta = delta_[grp];

  for (size_t i = 0; i < new_w.size(); ++i) {
    size_t j = col_range.begin() + i;
    double& cw = cur_w[j];
    double& nw = new_w[i];
    if (inactive(nw)) {
      // marked as inactive
      active_set.clear(j);
      cw = 0;
      delta_w[i] = 0;
      continue;
    }
    delta_w[i] = nw - cw;
    delta[j] = newDelta(delta_w[i]);
    cw = nw;
  }

  CHECK(X_[grp]);
  SizeR row_range(0, X_[grp]->rows());
  ThreadPool pool(FLAGS_num_threads);
  int npart = FLAGS_num_threads;
  for (int i = 0; i < npart; ++i) {
    auto thr_range = row_range.evenDivide(npart, i);
    if (thr_range.empty()) continue;
    pool.add([this, grp, thr_range, col_range, delta_w]() {
        updateDual(grp, thr_range, col_range, delta_w);
      });
  }
  pool.startWorkers();
}

void Darling::updateDual(
    int grp, SizeR row_range, SizeR col_range, SArray<double> w_delta) {
  CHECK_EQ(w_delta.size(), col_range.size());
  CHECK(X_[grp]->colMajor());

  const auto& active_set = active_set_[grp];
  double* y = y_->value().data();
  auto X = std::static_pointer_cast<
    SparseMatrix<uint32, double>>(X_[grp]->colBlock(col_range));
  size_t* offset = X->offset().data();
  uint32* index = X->index().data() + offset[0];
  double* value = X->value().data();
  bool binary = X->binary();

  // j: column id, i: row id
  for (size_t j = 0; j < X->cols(); ++j) {
    size_t k  = j + col_range.begin();
    size_t n = offset[j+1] - offset[j];
    double wd = w_delta[j];
    if (wd == 0 || !active_set.test(k)) {
      index += n;
      continue;
    }
    // TODO unroll the loop
    for (size_t o = offset[j]; o < offset[j+1]; ++o) {
      auto i = *(index++);
      if (!row_range.contains(i)) continue;
      dual_[i] *= binary ? exp(y[i] * wd) : exp(y[i] * wd * value[o]);
    }
  }
}

void Darling::updateWeight(
    int grp, SizeR range, SArray<double> G, SArray<double> U) {
  CHECK_EQ(G.size(), range.size());
  CHECK_EQ(U.size(), range.size());

  double eta = conf_.learning_rate().eta();
  double lambda = conf_.penalty().lambda(0);
  auto& value = w_->value(grp);
  auto& active_set = active_set_[grp];
  auto& delta = delta_[grp];
  for (size_t i = 0; i < range.size(); ++i) {
    size_t k = i + range.begin();
    if (!active_set.test(k)) continue;
    double g = G[i], u = U[i] / eta + 1e-10;
    double g_pos = g + lambda, g_neg = g - lambda;
    double& w = value[k];
    double d = - w, vio = 0;

    if (w == 0) {
      if (g_pos < 0) {
        vio = - g_pos;
      } else if (g_neg > 0) {
        vio = g_neg;
      } else if (g_pos > KKT_filter_threshold_ && g_neg < - KKT_filter_threshold_) {
        active_set.clear(k);
        w = kInactiveValue_;
        continue;
      }
    }
    violation_ = std::max(violation_, vio);

    if (g_pos <= u * w) {
      d = - g_pos / u;
    } else if (g_neg >= u * w) {
      d = - g_neg / u;
    }
    d = std::min(delta[k], std::max(-delta[k], d));
    delta[k] = newDelta(d);
    w += d;
  }
}


void Darling::showKKTFilter(int iter) {
  if (iter == -3) {
    fprintf(stderr, "|      KKT filter     ");
  } else if (iter == -2) {
    fprintf(stderr, "| threshold  #activet ");
  } else if (iter == -1) {
    fprintf(stderr, "+---------------------");
  } else {
    auto prog = g_progress_[iter];
    fprintf(stderr, "| %.1e %11llu ", KKT_filter_threshold_, (uint64)prog.nnz_active_set());
  }
}

void Darling::showProgress(int iter) {
  int s = iter == 0 ? -3 : iter;
  for (int i = s; i <= iter; ++i) {
    showObjective(i);
    showNNZ(i);
    showKKTFilter(i);
    showTime(i);
  }
}

Progress Darling::evaluateProgress() {
  Progress prog;
  if (IamWorker()) {
    prog.set_objv(log(1+1/dual_.eigenArray()).sum());
    prog.add_busy_time(busy_timer_.stop());
    busy_timer_.restart();

    // label statistics
    if (FLAGS_verbose) {
      size_t positive_label_count = 0;
      size_t negative_label_count = 0;
      size_t bad_label_count = 0;

      for (size_t i = 0; i < y_->value().size(); ++i) {
        int label = y_->value()[i];

        if (1 == label) {
          positive_label_count++;
        } else if (-1 == label) {
          negative_label_count++;
        } else {
          bad_label_count++;
        }
      }

      LI << "dual_sum[" << dual_.eigenArray().sum() << "] " <<
        "dual_.rows[" << dual_.eigenArray().rows() << "] " <<
        "dual_.avg[" << dual_.eigenArray().sum() / static_cast<double>(
          dual_.eigenArray().rows()) << "] " <<
        "y_.positive[" << positive_label_count << "] " <<
        "y_.negative[" << negative_label_count << "] " <<
        "y_.bad[" << bad_label_count << "] " <<
        "y_.positive_ratio[" << positive_label_count / static_cast<double>(
          positive_label_count + negative_label_count + bad_label_count) << "] ";
    }
  } else {
    size_t nnz_w = 0;
    size_t nnz_as = 0;
    double objv = 0;
    for (int grp : fea_grp_) {
      const auto& value = w_->value(grp);
      for (double w : value) {
        if (inactive(w) || w == 0) continue;
        ++ nnz_w;
        objv += fabs(w);
      }
      nnz_as += active_set_[grp].nnz();
    }
    prog.set_objv(objv * conf_.penalty().lambda(0));
    prog.set_nnz_w(nnz_w);
    prog.set_violation(violation_);
    prog.set_nnz_active_set(nnz_as);
  }
  return prog;
}

} // namespace LM
} // namespace PS
