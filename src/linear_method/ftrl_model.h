#pragma once
#include <cmath>
#include "parameter/shared_parameter.h"
#include "base/soft_thresholding.h"

namespace PS {
namespace LM {

template<typename K, typename V> class FTRLModel;
template<typename K, typename V> using FTRLModelPtr = std::shared_ptr<FTRLModel<K, V>>;

template <typename K, typename V>
class FTRLModel : public SharedParameter<K> {
 public:
  void setLearningRate(const LearningRateConfig& lr) {
    alpha_ = lr.alpha();
    beta_ = lr.beta();
  }
  void setPenalty(const PenaltyConfig& c) {
    if (c.lambda_size() > 0) lambda1_ = c.lambda(0);
    if (c.lambda_size() > 1) lambda2_ = c.lambda(1);
  }

  void writeToFile(const DataConfig& data) {
    std::ofstream out(data.file(0)); CHECK(out.good());
    for (const auto& it : model_) {
      if (it.second.w != 0) out << it.first << "\t" << it.second.w << "\n";
    }
  }

  V objv() { return norm1_ * lambda1_ + .5 * lambda2_ * sqrt(norm2_); }
  size_t nnz() { return nnz_; }

 public:  // implement required virtual functions
  MessagePtrList slice(const MessagePtr& msg, const KeyList& sep) {
    return sliceKeyOrderedMsg<K>(msg, sep);
  }
  void getValue(const MessagePtr& msg);
  void setValue(const MessagePtr& msg);

  // TODO fault tolerance
  void setReplica(const MessagePtr& msg) { }
  void getReplica(const MessagePtr& msg) { }
  void recoverFrom(const MessagePtr& msg) { }
 protected:
  // learning rate
  V alpha_, beta_;
  // penalty
  V lambda1_ = 0, lambda2_ = 0;
  struct Entry {
    // TODO use variable length integers, support multi-model and Q2.13 float
    uint64 pos = 0;
    uint64 neg = 0;
    V w = 0;
    V z = 0;
  };
  std::unordered_map<K, Entry> model_;

  // approximate sqrt(sum g_i^2) using feature counts. See sec 4.5
  V grad_norm(V pos, V neg) {
    V all = pos + neg;
    return all == 0 ? 0 : sqrt(pos * neg / all);
  }

  // status
  V norm1_ = 0;
  V norm2_ = 0;
  size_t nnz_ = 0;
};

template <typename K, typename V>
void FTRLModel<K,V>::getValue(const MessagePtr& msg) {
  SArray<K> key(msg->key);
  size_t n = key.size();
  SArray<V> val(n);
  for (size_t i = 0; i < n; ++i) val[i] = model_[key[i]].w;
  msg->addValue(val);
}

template <typename K, typename V>
void FTRLModel<K,V>::setValue(const MessagePtr& msg) {
  SArray<K> key(msg->key);
  size_t n = key.size();

  CHECK_EQ(msg->value.size(), 3);
  SArray<uint32> pos(msg->value[0]); CHECK_EQ(pos.size(), n);
  SArray<uint32> neg(msg->value[1]); CHECK_EQ(neg.size(), n);
  SArray<V> grad(msg->value[2]); CHECK_EQ(grad.size(), n);

  for (size_t i = 0; i < n; ++i) {
    // update model
    auto& e = model_[key[i]];
    V sqrt_n = grad_norm(e.pos, e.neg);
    e.pos += pos[i]; e.neg += neg[i];
    V sqrt_n_new = grad_norm(e.pos, e.neg);
    V sigma = (sqrt_n_new - sqrt_n) / alpha_;
    e.z += grad[i]  - sigma * e.w;
    V lambda2 = lambda2_ + (beta_ + sqrt_n_new) / alpha_;
    V w_old = e.w;
    V w = -softThresholding(e.z, lambda1_, lambda2);
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
