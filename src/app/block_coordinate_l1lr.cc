#include "app/block_coordinate_l1lr.h"
#include "base/sparse_matrix.h"
namespace PS {

// quite similar to LinearBlockIterator::run(), but diffs at the KKT filter
void BlockCoordinateL1LR::run() {
  LinearMethod::startSystem();
  auto blocks = LinearBlockIterator::partitionFeatures();

  std::vector<int> block_order;
  for (int i = 0; i < blocks.size(); ++i) block_order.push_back(i);
  auto cf = app_cf_.block_iterator();
  KKT_filter_threshold_ = 1e20;
  bool reset_kkt_filter = false;

  // iterating
  auto pool = taskpool(kActiveGroup);
  int time = pool->time();
  int tau = cf.max_block_delay();
  int iter = 0;
  for (; iter < cf.max_pass_of_data(); ++iter) {
    if (cf.random_feature_block_order())
      std::random_shuffle(block_order.begin(), block_order.end());
    for (int b : block_order)  {
      Task update;
      update.set_wait_time(time - tau);
      auto cmd = RiskMinimization::setCall(&update);
      if (b == block_order[0]) {
        cmd->set_kkt_filter_threshold(KKT_filter_threshold_);
      }
      if (reset_kkt_filter) {
        cmd->set_kkt_filter_reset(true);
      }
      cmd->set_cmd(RiskMinCall::UPDATE_MODEL);
      blocks[b].second.to(cmd->mutable_key());
      cmd->set_feature_group_id(blocks[b].first);
      time = pool->submit(update);
    }

    Task eval;
    RiskMinimization::setCall(&eval)->set_cmd(RiskMinCall::EVALUATE_PROGRESS);
    eval.set_wait_time(time - tau);

    time = pool->submit(eval, [this, iter](){ RiskMinimization::mergeProgress(iter); });
    pool->waitOutgoingTask(time);

    showProgress(iter);

    double vio = global_progress_[iter].violation();
    KKT_filter_threshold_ = vio / (double)global_training_example_size_
                            * app_cf_.block_coord_l1lr().kkt_filter_threshold_ratio();

    if (global_progress_[iter].relative_objv() <= cf.epsilon()) {
      fprintf(stderr, "convergence criteria satisfied: relative objective <= %.1e\n", cf.epsilon());
      break;
    }
  }

  if (iter == cf.max_pass_of_data()) {
    fprintf(stderr, "reached maximal # of data pass: %d\n", cf.max_pass_of_data());
  }
}



void BlockCoordinateL1LR::prepareData(const Message& msg) {
  LinearBlockIterator::prepareData(msg);
  if (exec_.isWorker()) {
    // dual_ = exp(y.*(X_*w_))
    dual_.eigenArray() = exp(y_->value().eigenArray() * dual_.eigenArray());
  }

  active_set_.resize(w_->size(), true);

  l1lr_cf_ = app_cf_.block_coord_l1lr();

  delta_.resize(w_->size());
  delta_.setValue(l1lr_cf_.delta_init_value());

  training_auc_.setGoodness(l1lr_cf_.auc_goodness());
}

void BlockCoordinateL1LR::updateModel(Message* msg) {
  auto time = msg->task.time() * 10;
  auto call = msg->task.risk();
  if (call.has_kkt_filter_threshold()) {
    KKT_filter_threshold_ = call.kkt_filter_threshold();
    violation_ = 0;
  }
  Range<Key> global_range(call.key());
  auto local_range = w_->localRange(global_range);
  int num_threads = FLAGS_num_threads;
  CHECK_GT(num_threads, 0);

  if (exec_.isWorker()) {
    busy_timer_.start();

    // compute local gradients
    SArrayList<double> local_grads(2);
    for (int i : {0, 1} ) {
      local_grads[i].resize(local_range.size());
      local_grads[i].setZero();
    }
    {
      Lock lk(mu_);
      if (l1lr_cf_.enable_multi_thread()) {
        ThreadPool pool(num_threads);
        int npart = num_threads * 1;
        for (int i = 0; i < npart; ++i) {
          auto range = local_range.evenDivide(npart, i);
          if (range.empty()) continue;
          auto grad_r = range - local_range.begin();
          pool.add([this, range, grad_r, &local_grads, i, npart]() {
              computeGradients(range, local_grads[0].segment(grad_r), local_grads[1].segment(grad_r));
            });
        }
        pool.startWorkers();
      } else {
        computeGradients(local_range, local_grads[0], local_grads[1]);
      }
    }

    busy_timer_.stop();

    msg->finished = false;
    auto d = *msg;
    w_->roundTripForWorker(time, global_range, local_grads,
                           [this, local_range, d, num_threads] (int time) {
        // update dual variable
        Lock l(mu_);
        busy_timer_.start();

        if (!local_range.empty()) {
          auto data = w_->received(time);
          CHECK_EQ(data.size(), 1);
          CHECK_EQ(local_range, data[0].first);
          auto new_weight = data[0].second;
          SArray<double> delta_w(new_weight.size());
          for (size_t i = 0; i < new_weight.size(); ++i) {
            size_t j = local_range.begin() + i;
            double& old_w = w_->value()[j];
            double& new_w = new_weight[i];
            if (new_w != new_w) {
              active_set_.clear(j);
              old_w = new_w = 0;
            } else {
              active_set_.set(j);
            }
            delta_w[i] = new_w - old_w;
            delta_[j] = std::min(
                l1lr_cf_.delta_max_value(), 2 * fabs(delta_w[i]) + .1);
            old_w = new_w;
          }
          {
            SizeR example_range(0, X_->rows());
            if (l1lr_cf_.enable_multi_thread()) {
              ThreadPool pool(num_threads);
              int npart = num_threads;
              for (int i = 0; i < npart; ++i) {
                auto range = example_range.evenDivide(npart, i);
                if (range.empty()) continue;
                pool.add([this, range, local_range,  &delta_w]() {
                    updateDual(range, local_range, delta_w);
                  });
              }
              pool.startWorkers();
            } else {
              updateDual(example_range, local_range, delta_w);
            }
          }
        }

        busy_timer_.stop();
        taskpool(d.sender)->finishIncomingTask(d.task.time());
        sys_.reply(d);
        // LL << myNodeID() << " done " << d.task.time();
      });
  } else {
    // aggregate local gradients, then update model via soft-shrinkage
    w_->roundTripForServer(time, global_range, [this, local_range] (int time) {
        if (local_range.empty()) return;
        auto data = w_->received(time);
        CHECK_EQ(data.size(), 2);
        CHECK_EQ(local_range, data[0].first);
        CHECK_EQ(local_range, data[1].first);

        updateWeight(local_range, data[0].second, data[1].second);
      });
  }
}

void BlockCoordinateL1LR::computeGradients(
    SizeR local_feature_range, SArray<double> G, SArray<double> U) {
  CHECK_EQ(G.size(), local_feature_range.size());
  CHECK_EQ(U.size(), local_feature_range.size());
  CHECK(!X_->rowMajor());

  const double* y = y_->value().data();
  auto X = std::static_pointer_cast<SparseMatrix<uint32, double>>(
      X_->colBlock(local_feature_range));
  const size_t* offset = X->offset().data();
  uint32* index = X->index().data() + offset[0];
  double* value = X->value().data() + offset[0];
  bool binary = X->binary();

  // j: column id, i: row id
  for (size_t j = 0; j < X->cols(); ++j) {
    size_t k = j + local_feature_range.begin();
    size_t n = offset[j+1] - offset[j];
    if (!active_set_.test(k)) {
      index += n;
      if (!binary) value += n;
      continue;
    }
    double g = 0, u = 0;
    double d = binary ? exp(delta_[k]) : delta_[k];
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


void BlockCoordinateL1LR::updateDual(
    SizeR local_example_range, SizeR local_feature_range,
    SArray<double> w_delta) {
  CHECK_EQ(w_delta.size(), local_feature_range.size());
  CHECK(!X_->rowMajor());

  auto X = std::static_pointer_cast<SparseMatrix<uint32, double>>(
      X_->colBlock(local_feature_range));
  double* y = y_->value().data();
  size_t* offset = X->offset().data();
  uint32* index = X->index().data() + offset[0];
  double* value = X->value().data();
  bool binary = X->binary();

  // j: column id, i: row id
  for (size_t j = 0; j < X->cols(); ++j) {
    size_t k  = j + local_feature_range.begin();
    size_t n = offset[j+1] - offset[j];
    double wd = w_delta[j];
    if (wd == 0 || !active_set_.test(k)) {
      index += n;
      continue;
    }
    // TODO unroll the loop
    for (size_t o = offset[j]; o < offset[j+1]; ++o) {
      auto i = *(index++);
      if (!local_example_range.contains(i)) continue;
      dual_[i] *= binary ? exp(y[i] * wd) : exp(y[i] * wd * value[o]);
    }
  }
}

void BlockCoordinateL1LR::updateWeight(
    SizeR local_feature_range, const SArray<double>& G, const SArray<double>& U) {
  CHECK_EQ(G.size(), local_feature_range.size());
  CHECK_EQ(U.size(), local_feature_range.size());

  double eta = app_cf_.learner().learning_rate();
  double lambda = app_cf_.penalty().coefficient();

  for (size_t i = 0; i < local_feature_range.size(); ++i) {
    size_t k = i + local_feature_range.begin();
    double g = G[i], u = U[i] / eta + 1e-10;
    double g_pos = g + lambda, g_neg = g - lambda;
    double& w = w_->value()[k];
    double d = - w, vio = 0;

    if (w == 0) {
      if (g_pos < 0) {
        vio = - g_pos;
      } else if (g_neg > 0) {
        vio = g_neg;
      } else if (g_pos > KKT_filter_threshold_ && g_neg < - KKT_filter_threshold_) {
        active_set_.clear(k);
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

    d = std::min(delta_[k], std::max(-delta_[k], d));

    w += d;
    delta_[k] = std::min(l1lr_cf_.delta_max_value(), 2 * fabs(d) + .1);
  }
}


void BlockCoordinateL1LR::showKKTFilter(int iter) {
  if (iter == -3) {
    fprintf(stderr, "|      KKT filter     ");
  } else if (iter == -2) {
    fprintf(stderr, "| threshold  #activet ");
  } else if (iter == -1) {
    fprintf(stderr, "+---------------------");
  } else {
    auto prog = global_progress_[iter];
    fprintf(stderr, "| %.1e %11llu ", KKT_filter_threshold_, prog.nnz_active_set());
  }
}

void BlockCoordinateL1LR::showProgress(int iter) {
  int s = iter == 0 ? -3 : iter;
  for (int i = s; i <= iter; ++i) {
    RiskMinimization::showObjective(i);
    RiskMinimization::showNNZ(i);
    showKKTFilter(i);
    RiskMinimization::showTime(i);
  }
}

RiskMinProgress BlockCoordinateL1LR::evaluateProgress() {
  RiskMinProgress prog;
  if (exec_.isWorker()) {
    prog.set_objv(log(1+1/dual_.eigenArray()).sum());
    prog.add_busy_time(busy_timer_.get());
    SArray<double> predict(dual_.size());
    // = X * w
    predict.eigenArray() = y_->value().eigenArray() * log(dual_.eigenArray());
    training_auc_.compute(y_->value(), predict, prog.mutable_training_auc_data());
  } else {
    size_t nnz_w = 0;
    double objv = 0;
    for (size_t i = 0; i < w_->size(); ++i) {
      auto w = w_->value()[i];
      if (w == 0 || w != w) continue;
      ++ nnz_w;
      objv += fabs(w);
    }
    prog.set_objv(objv * app_cf_.penalty().coefficient());
    prog.set_nnz_w(nnz_w);
    prog.set_violation(violation_);
    prog.set_nnz_active_set(active_set_.nnz());
  }
  return prog;
}

} // namespace PS
