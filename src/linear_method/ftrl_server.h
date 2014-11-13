#pragma once
#include "linear_method/ftrl.h"
#include "parameter/kv_map.h"
namespace PS {
namespace LM {

struct FTRLEntry {
  Real w = 0;  // not necessary to store w, because it can be computed from z
  Real z = 0;
  Real sqrt_n = 0;
};

class FTRLServer : public KVMap<Key, FTRLEntry> {
 public:
  void init(const Config& conf) {
    // set learning rate
    alpha_ = conf.learning_rate().alpha();
    beta_ = conf.learning_rate().beta();

    // set penalty
    if (conf_.penalty().lambda_size() > 0) lambda1_ = conf_.penalty().lambda(0);
    if (conf_.penalty().lambda_size() > 1) lambda2_ = conf_.penalty().lambda(1);

    // tail feature filter
    key_filter_[0].resize(
        conf.solver().countmin_n()/FLAGS_num_servers, conf.solver().countmin_k());
    key_filter_ignore_chl_ = true;
  }

  void evaluateProgress(Progress* prog) {
    prog.set_objv(norm1_ * lambda1_ + .5 * lambda2_ * sqrt(norm2_));
    prog.set_nnz_w(nnz_);
  }

  void writeToFile(const DataConfig& data) {
    std::ofstream out(data.file(0)); CHECK(out.good());
    for (const auto& it : data_) {
      if (it.second.w != 0) out << it.first << "\t" << it.second.w << "\n";
    }
  }

  // overide the default setValue and getValue functions
  void setValue(const MessagePtr& msg);
  void getValue(const MessagePtr& msg);

 protected:
  // learning rate
  V alpha_, beta_;

  // penalty
  V lambda1_ = 0, lambda2_ = 0;

  // status
  V norm1_ = 0;
  V norm2_ = 0;
  size_t nnz_ = 0;
};

void FTRLModel::getValue(const MessagePtr& msg) {
  SArray<Key> key(msg->key);
  size_t n = key.size();
  SArray<Real> val(n);
  for (size_t i = 0; i < n; ++i) {
    val[i] = data_[key[i]].w;
  }
  msg->addValue(val);
}

void FTRLModel::setValue(const MessagePtr& msg) {
  SArray<Key> key(msg->key);
  size_t n = key.size();
  CHECK_EQ(msg->value.size(), 1);
  SArray<Real> grad(msg->value[0]);
  CHECK_EQ(grad.size(), n);

  for (size_t i = 0; i < n; ++i) {
    // update model
    auto& e = data_[key[i]];
    Real sqrt_n_new = sqrt(e.sqrt_n * e.sqrt_n + grad[i]*grad[i]);
    Real sigma = (sqrt_n_new - e.sqrt_n) / alpha_;
    e.z += grad[i]  - sigma * e.w;
    e.sqrt_n = sqrt_n_new;
    V lambda2 = lambda2_ + (beta_ + sqrt_n_new) / alpha_;
    Real w = -softThresholding(e.z, lambda1_, lambda2);
    Real w_old = e.w;
    e.w = w;

    // update status
    norm1_ += fabs(w) - fabs(w_old);
    norm2_ += w * w - w_old * w_old;
    if (w == 0 && w_old != 0) {
      -- nnz_;
    } else if (w != 0 && w_old == 0) {
      ++ nnz_;
    }
  }
}


} // namespace LM
} // namespace PS
