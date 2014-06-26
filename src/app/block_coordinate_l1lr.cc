#include "app/block_coordinate_l1lr.h"
#include "base/sparse_matrix.h"
namespace PS {

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


// quite similar to LinearBlockIterator::run(), but diffs at the KKT filter
void BlockCoordinateL1LR::run() {
  LinearMethod::startSystem();
  auto blocks = LinearBlockIterator::partitionFeatures();

  std::vector<int> block_order;
  for (int i = 0; i < blocks.size(); ++i) block_order.push_back(i++);
  auto cf = app_cf_.block_iterator();
  KKT_filter_threshold_ = 1e20;
  bool reset_kkt_filter = false;

  // iterating
  auto wk = taskpool(kActiveGroup);
  int time = wk->time();
  int tau = cf.max_block_delay();
  for (int iter = 0; iter < cf.max_pass_of_data(); ++iter) {
    std::random_shuffle(block_order.begin(), block_order.end());

    for (int b : block_order)  {
      Task update;
      update.set_wait_time(time - tau);
      auto cmd = RiskMinimization::setCall(&update);
      cmd->set_kkt_filter_threshold(KKT_filter_threshold_);
      if (reset_kkt_filter) cmd->set_kkt_filter_reset(true);
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

    showProgress(iter);

    double vio = global_progress_[iter].violation();
    KKT_filter_threshold_ = vio;

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
  LL << active_set_.nnz();
  delta_.resize(w_->size());
  delta_.setValue(app_cf_.block_coord_l1lr().delta_init_value());
}

void BlockCoordinateL1LR::updateModel(Message* msg) {
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
    {
      Lock lk(mu_);
      busy_timer_.start();

      computeGradients(local_range, local_grads[0], local_grads[1]);

      busy_timer_.stop();
    }

    msg->finished = false;
    auto d = *msg;
    w_->roundTripForWorker(time, global_range, local_grads, [this, local_range, d] (int time) {
        // update dual variable
        Lock l(mu_);
        busy_timer_.start();

        if (!local_range.empty()) {
          auto data = w_->received(time);
          CHECK_EQ(data.size(), 1);
          CHECK_EQ(local_range, data[0].first);
          auto new_w = data[0].second;
          SArray<double> delta_w(new_w.size());
          for (size_t i = 0; i < new_w.size(); ++i) {
            size_t j = local_range.begin() + i;
            if (new_w[i] == kInactiveValue_) {
              active_set_.clear(j);
              new_w[i] = 0;
            } else {
              active_set_.set(j);
            }
            delta_w[i] = new_w[i] - w_->value()[j];
            delta_[j] = std::min(
                app_cf_.block_coord_l1lr().delta_max_value(), 2 * fabs(delta_w[i]) + .1);
            w_->value()[j] = new_w[i];
          }
          SizeR example_range(0, X_->rows());
          updateDual(example_range, local_range, delta_w);
        }

        busy_timer_.stop();
        taskpool(d.sender)->finishIncomingTask(d.task.time());
        sys_.reply(d);
        // LL << myNodeID() << " done " << d.task.time();
      });
  } else {
    // aggregate local gradients, then update model via soft-shrinkage
    w_->roundTripForServer(time, global_range, [this, local_range] (int time) {

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

  auto X = std::static_pointer_cast<SparseMatrix<uint32, double>>(
      X_->colBlock(local_feature_range));
  const auto& offset = X->offset();
  const auto& index = X->index();
  bool binary = X->binary();

  // j: column id, i: row id
  for (size_t j = 0; j < offset.size()-1; ++j) {
    size_t k  = j + local_feature_range.begin();
    if (!active_set_.test(k) || offset[j] == offset[j+1]) continue;
    double g = 0, u = 0, d = delta_[k];
    for (size_t o = offset[j]; o < offset[j+1]; ++o) {
      // auto i = X->index()[o];
      auto i = index[o];
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

  auto X = std::static_pointer_cast<SparseMatrix<uint32, double>>(
      X_->colBlock(local_feature_range));
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
    double w = w_->value()[k];
    double d = - w, vio = 0;

    if (w == 0) {
      if (g_pos < 0) {
        vio = - g_pos;
      } else if (g_neg > 0) {
        vio = g_neg;
      } else if (g_pos > KKT_filter_threshold_ && g_neg < - KKT_filter_threshold_) {
        active_set_.clear(k);
        w_->value()[k] = kInactiveValue_;
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

    w_->value()[k] += d;
    delta_[k] = std::min(
        app_cf_.block_coord_l1lr().delta_max_value(), 2 * fabs(d) + .1);
  }

}


} // namespace PS
