#include "linear_method/darlin_worker.h"
#include "base/sparse_matrix.h"

namespace PS {
namespace LM {

void DarlinWorker::preprocessData(const MessagePtr& msg) {
  BatchWorker::preprocessData(msg);
  // dual_ = exp(y.*(X_*w_))
  if (conf_.init_w().type() == ParameterInitConfig::ZERO) {
    dual_.setValue(1);  // an optimizatoin
  } else {
    dual_.eigenArray() = exp(y_->value().eigenArray() * dual_.eigenArray());
  }
  for (int grp : fea_grp_) {
    size_t n = model_->key(grp).size();
    active_set_[grp].resize(n, true);
    delta_[grp].resize(n, conf_.darling().delta_init_value());
  }
}

void DarlinWorker::computeGradient(const MessagePtr& msg) {
  int time = msg->task.time() * k_time_ratio_;
  auto cmd = LinearMethod::get(msg);
  if (cmd.reset_kkt_filter()) {
    for (int grp : fea_grp_) active_set_[grp].fill(true);
  }
  CHECK_EQ(cmd.fea_grp_size(), 1);
  int grp = cmd.fea_grp(0);
  Range<Key> g_key_range(cmd.key());
  auto col_range = model_->find(grp, g_key_range);

  // compute and push the gradient
  computeAndPushGradient(time, g_key_range, grp, col_range);

  // pull the updated model, and update dual
  pullAndUpdateDual(time+2, g_key_range, grp, col_range, msg);

  // this task is not finished until the updated model is pulled
  msg->finished = false;
}


void DarlinWorker::pullAndUpdateDual(
    int time, Range<Key> g_key_range, int grp, SizeR col_range,
    const MessagePtr& msg) {
  // pull the updated weight from the server
  MessagePtr pull(new Message(kServerGroup, time, time-1));
  pull->setKey(model_->key(grp).segment(col_range));
  g_key_range.to(pull->task.mutable_key_range());
  pull->task.set_key_channel(grp);
  pull->addFilter(FilterConfig::KEY_CACHING);

  // once data pulled successfully, update dual_
  pull->fin_handle = [this, time, grp, col_range, msg](){
    if (!col_range.empty()) {
      auto data = model_->received(time);
      CHECK_EQ(col_range, data.first);
      CHECK_EQ(data.second.size(), 1);
      updateDual(grp, col_range, data.second[0]);
    }

    // mark the message finished, and reply the sender
    taskpool(msg->sender)->finishIncomingTask(msg->task.time());
    sys_.reply(msg->sender, msg->task);
  };

  CHECK_EQ(time, model_->pull(pull));
}

void DarlinWorker::computeAndPushGradient(
    int time, Range<Key> g_key_range, int grp, SizeR col_range) {
  SArray<double> G(col_range.size(), 0);
  SArray<double> U(col_range.size(), 0);

  mu_.lock();  // lock the dual_
  sys_.hb().startTimer(HeartbeatInfo::TimerType::BUSY);
  busy_timer_.start();
  // compute the gradient in multi-thread
  if (!col_range.empty()) {
    CHECK_GT(FLAGS_num_threads, 0);
    // TODO partition by rows for small col_range size
    int num_threads = col_range.size() < 64 ? 1 : FLAGS_num_threads;
    ThreadPool pool(num_threads);
    int npart = num_threads * 1;  // could use a larger partition number
    for (int i = 0; i < npart; ++i) {
      auto thr_range = col_range.evenDivide(npart, i);
      if (thr_range.empty()) continue;
      auto gr = thr_range - col_range.begin();
      pool.add([this, grp, thr_range, gr, &G, &U]() {
          computeGradient(grp, thr_range, G.segment(gr), U.segment(gr));
        });
    }
    pool.startWorkers();
  }
  busy_timer_.stop();
  sys_.hb().stopTimer(HeartbeatInfo::TimerType::BUSY);
  mu_.unlock();  // lock the dual_

  // push the gradient into servers
  MessagePtr push(new Message(kServerGroup, time));
  push->setKey(model_->key(grp).segment(col_range));
  push->addValue({G, U});
  g_key_range.to(push->task.mutable_key_range());
  push->task.set_key_channel(grp);
  push->addFilter(FilterConfig::KEY_CACHING);
  CHECK_EQ(time, model_->push(push));
}

void DarlinWorker::computeGradient(
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
      kkt_filter_.mark(&G[j]);
      kkt_filter_.mark(&U[j]);
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

void DarlinWorker::updateDual(int grp, SizeR col_range, SArray<double> new_w) {
  auto& cur_w = model_->value(grp);
  auto& active_set = active_set_[grp];
  auto& delta = delta_[grp];

  double delta_max = conf_.darling().delta_max_value();
  SArray<double> delta_w(new_w.size());
  for (size_t i = 0; i < new_w.size(); ++i) {
    size_t j = col_range.begin() + i;
    double& cw = cur_w[j];
    double& nw = new_w[i];
    if (kkt_filter_.marked(nw)) {
      active_set.clear(j);
      cw = 0;
      delta_w[i] = 0;
      continue;
    }
    delta_w[i] = nw - cw;
    delta[j] = newDelta(delta_max, delta_w[i]);
    cw = nw;
  }

  CHECK(X_[grp]);

  mu_.lock();  // lock the dual_
  sys_.hb().startTimer(HeartbeatInfo::TimerType::BUSY);
  busy_timer_.start();
  {
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
  busy_timer_.stop();
  sys_.hb().stopTimer(HeartbeatInfo::TimerType::BUSY);
  mu_.unlock();  // lock the dual_
}

void DarlinWorker::updateDual(
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

void DarlinWorker::evaluateProgress(Progress* prog) {
  busy_timer_.start();
  mu_.lock();  // lock the dual_
  prog->add_objv(log(1+1/dual_.eigenArray()).sum());
  mu_.unlock();
  prog->add_busy_time(busy_timer_.stop());
  busy_timer_.restart();

  // double mean = 0;
  // double* y = y_->value().data();
  // for (int i = 0; i < dual_.size(); ++i) {
  //   double q = 1 / (1 + dual_[i]);
  //   mean += y[i] == 1 ? q : 1 - q;
  // }
  // LL << "average predicted prob: " << 1 - mean / dual_.size();

  // // label statistics
  // if (FLAGS_verbose) {
  //   size_t positive_label_count = 0;
  //   size_t negative_label_count = 0;
  //   size_t bad_label_count = 0;

  //   for (size_t i = 0; i < y_->value().size(); ++i) {
  //     int label = y_->value()[i];

  //     if (1 == label) {
  //       positive_label_count++;
  //     } else if (-1 == label) {
  //       negative_label_count++;
  //     } else {
  //       bad_label_count++;
  //     }
  //   }

  //   LI << "dual_sum[" << dual_.eigenArray().sum() << "] " <<
  //       "dual_.rows[" << dual_.eigenArray().rows() << "] " <<
  //       "dual_.avg[" << dual_.eigenArray().sum() / static_cast<double>(
  //           dual_.eigenArray().rows()) << "] " <<
  //       "y_.positive[" << positive_label_count << "] " <<
  //       "y_.negative[" << negative_label_count << "] " <<
  //       "y_.bad[" << bad_label_count << "] " <<
  //       "y_.positive_ratio[" << positive_label_count / static_cast<double>(
  //           positive_label_count + negative_label_count + bad_label_count) << "] ";

  // }
}

} // namespace LM
} // namespace PS
