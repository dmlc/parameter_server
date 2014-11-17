#pragma once
#include "linear_method/ftrl_common.h"
#include "parameter/shared_parameter.h"
#include "base/soft_thresholding.h"
namespace PS {
namespace LM {

struct FTRLEntry {
  real w = 0;  // not necessary to store w, because it can be computed from z
  real z = 0;
  real sqrt_n = 0;
};

class FTRLModel : public SharedParameter<Key> {
 public:
  void init(const Config& conf) {
    // set learning rate
    alpha_ = conf.learning_rate().alpha();
    beta_ = conf.learning_rate().beta();

    // set penalty
    if (conf.penalty().lambda_size() > 0) lambda1_ = conf.penalty().lambda(0);
    if (conf.penalty().lambda_size() > 1) lambda2_ = conf.penalty().lambda(1);

    // tail feature filter
    key_filter_[0].resize(
        conf.solver().countmin_n()/FLAGS_num_servers, conf.solver().countmin_k());
    key_filter_ignore_chl_ = true;
  }

  void evaluateProgress(Progress* prog) {
    // prog->set_objv(norm1_ * lambda1_ + .5 * lambda2_ * sqrt(norm2_));
    prog->set_nnz_w(nnz_);
  }

  void writeToFile(const DataConfig& data) {
    std::ofstream out(data.file(0)); CHECK(out.good());
    for (const auto& it : model_) {
      if (it.second.w != 0) out << it.first << "\t" << it.second.w << "\n";
    }
  }

  void setValue(const MessagePtr& msg);
  void getValue(const MessagePtr& msg);

  virtual MessagePtrList slice(const MessagePtr& msg, const KeyList& sep) {
    return sliceKeyOrderedMsg<Key>(msg, sep);
  }

 protected:
  std::unordered_map<Key, FTRLEntry> model_;

  // learning rate
  real alpha_, beta_;

  // penalty
  real lambda1_ = 0, lambda2_ = 0;

  // status
  // real norm1_ = 0;
  // real norm2_ = 0;
  size_t nnz_ = 0;
};


void FTRLModel::getValue(const MessagePtr& msg) {
  SArray<Key> key(msg->key);
  size_t n = key.size();
  SArray<real> val(n);
  for (size_t i = 0; i < n; ++i) {
    val[i] = model_[key[i]].w;
  }
  msg->addValue(val);
}

void FTRLModel::setValue(const MessagePtr& msg) {
  SArray<Key> key(msg->key);
  size_t n = key.size();
  CHECK_EQ(msg->value.size(), 1);
  SArray<real> grad(msg->value[0]);
  CHECK_EQ(grad.size(), n);

  for (size_t i = 0; i < n; ++i) {
    // update model
    auto& e = model_[key[i]];
    real sqrt_n_new = sqrt(e.sqrt_n * e.sqrt_n + grad[i]*grad[i]);
    real sigma = (sqrt_n_new - e.sqrt_n) / alpha_;
    e.z += grad[i]  - sigma * e.w;
    e.sqrt_n = sqrt_n_new;
    real lambda2 = lambda2_ + (beta_ + sqrt_n_new) / alpha_;
    real w = - softThresholding(e.z, lambda1_, lambda2);
    real w_old = e.w;
    e.w = w;

    // update status
    // norm1_ += fabs(w) - fabs(w_old);
    // norm2_ += w * w - w_old * w_old;
    if (w == 0 && w_old != 0) {
      -- nnz_;
    } else if (w != 0 && w_old == 0) {
      ++ nnz_;
    }
  }
}


} // namespace LM
} // namespace PS
