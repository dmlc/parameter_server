#include "linear_method/darlin_server.h"
namespace PS {
namespace LM {

void DarlinServer::preprocessData(const MessagePtr& msg) {
  BatchServer::preprocessData(msg);
  for (int grp : fea_grp_) {
    size_t n = model_->key(grp).size();
    active_set_[grp].resize(n, true);
    delta_[grp].resize(n, conf_.darling().delta_init_value());
  }
}

void DarlinServer::updateWeight(const MessagePtr& msg) {
  int time = msg->task.time() * k_time_ratio_;
  auto cmd = get(msg);
  if (cmd.has_kkt_filter_threshold()) {
    kkt_filter_threshold_ = cmd.kkt_filter_threshold();
    violation_ = 0;
  }
  if (cmd.reset_kkt_filter()) {
    for (int grp : fea_grp_) active_set_[grp].fill(true);
  }
  CHECK_EQ(cmd.fea_grp_size(), 1);
  int grp = cmd.fea_grp(0);
  Range<Key> g_key_range(cmd.key());
  auto col_range = model_->find(grp, g_key_range);

  // none of my bussiness
  if (model_->myKeyRange().setIntersection(g_key_range).empty()) return;

  //  aggregate all workers' local gradients
  model_->waitInMsg(kWorkerGroup, time);

  // update the weights
  if (!col_range.empty()) {
    auto data = model_->received(time);
    CHECK_EQ(col_range, data.first);
    CHECK_EQ(data.second.size(), 2);

    sys_.hb().startTimer(HeartbeatInfo::TimerType::BUSY);
    updateWeight(grp, col_range, data.second[0], data.second[1]);
    sys_.hb().stopTimer(HeartbeatInfo::TimerType::BUSY);
  }

  model_->finish(kWorkerGroup, time+1);
}

void DarlinServer::updateWeight(
    int grp, SizeR range, SArray<double> G, SArray<double> U) {
  CHECK_EQ(G.size(), range.size());
  CHECK_EQ(U.size(), range.size());

  double eta = conf_.learning_rate().eta();
  double lambda = conf_.penalty().lambda(0);
  double delta_max = conf_.darling().delta_max_value();
  auto& value = model_->value(grp);
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
      } else if (g_pos > kkt_filter_threshold_ && g_neg < - kkt_filter_threshold_) {
        active_set.clear(k);
        kkt_filter_.mark(&w);
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
    delta[k] = newDelta(delta_max, d);
    w += d;
  }
}

void DarlinServer::evaluateProgress(Progress* prog) {
  size_t nnz_w = 0;
  size_t nnz_as = 0;
  double objv = 0;
  for (int grp : fea_grp_) {
    const auto& value = model_->value(grp);
    for (double w : value) {
      if (kkt_filter_.marked(w) || w == 0) continue;
      ++ nnz_w;
      objv += fabs(w);
    }
    nnz_as += active_set_[grp].nnz();
  }
  prog->add_objv(objv * conf_.penalty().lambda(0));
  prog->set_nnz_w(nnz_w);
  prog->set_violation(violation_);
  prog->set_nnz_active_set(nnz_as);

}

} // namespace LM
} // namespace PS
