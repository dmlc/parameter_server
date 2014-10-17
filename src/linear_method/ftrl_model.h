#pragma once
#include <cmath>
#include "parameter/shared_parameter.h"
#include "base/soft_thresholding.h"

namespace PS {
namespace LM {

template<typename K> class FTRLModel;
template<typename K> using FTRLModelPtr = std::shared_ptr<FTRLModel<K>>;

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
};

template <typename K, typename V>
void FTRLModel<K,V>::getValue(const MessagePtr& msg) {
  SArray<K> key(msg->key);
  size_t n = key.size();
  SArray<V> val(n);
  for (size_t i = 0; i < n; ++i) val[i] = model_[key[i]].weight;
  msg->addValue(val);
}

template <typename K, typename V>
void FTRLModel<K,V>::setValue(const MessagePtr& msg) {
  SArray<K> key(msg->key);
  size_t n = key.size();

  CHECK_EQ(msg->value.size(), 3);
  SArray<uint32> pos(msg->value(0)); CHECK_EQ(pos.size(), n);
  SArray<uint32> neg(msg->value(1)); CHECK_EQ(neg.size(), n);
  SArray<V> grad(msg->value(2)); CHECK_EQ(grad.size(), n);

  for (size_t i = 0; i < n; ++i) {
    auto& e = model_[key[i]];
    V sqrt_n = sqrt((V) e.pos * (V) e.neg / (V) (e.pos + e.neg));
    e.pos += pos[i]; e.neg += neg[i];
    V sqrt_n_new = sqrt((V) e.pos * (V) e.neg / (V) (e.pos + e.neg));
    V sigma = (sqrt_n_new - sqrt_n) / alpha;
    e.z += grad[i]  - sigma * e.w;
    e.w = -softThresholding(e.z, lambda1_, lambda2_ + (beta_ + sqrt_n_new) / alpha_);
  }
}

} // namespace LM
} // namespace PS
