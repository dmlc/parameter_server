#pragma once
#include "util/common.h"
#include "util/xarray.h"
#include "box/container.h"
#include "util/eigen3.h"

namespace PS {

template<class V>
class DenseVector : public Container {
 public:
  typedef Eigen::Matrix<V, Eigen::Dynamic, 1> EVec;
  // construct
  DenseVector(string name, size_t length);
  // accessor
  V& operator[] (const Key& key) { return local_[key]; }

  Status PushToServer(bool delta) {
    SyncFlag flag;
    flag.set_recver(NodeGroup::kServers);
    flag.set_push_delta(delta);
    flag.set_type(SyncFlag::PUSH);
    return Push(flag);
  }

  // Status PushPullToServer(bool push_delta, bool pull_delta) {
  //   SyncFlag flag;
  //   flag.set_recver(NodeGroup::kServers);
  //   flag.set_push_delta();
  //   flag.set_pull_delta(delta);
  //   flag.set_type(SyncFlag::PUSH);
  //   return Push(flag);
  // }
  Status GetLocalData(Mail *mail);
  Status MergeRemoteData(const Mail& mail);

  // get eigen3 vector
  EVec& vec() { return local_; }
  EVec& synced() { return synced_; }
  EVec& GetVec() { return local_; }

  // static void FreeUpdateBuff(Updates *update);
 private:
  // use an eigen3 dense vector as the internal container, it makes linear algebra easier
  EVec local_;
  EVec synced_;
  size_t len_;
  XArray<Key> keys_;
};

template<class V>
DenseVector<V>::DenseVector(string name, size_t length)
    : Container(name, 0, length), len_(length), keys_(length) {
  // the resize do not set entries to 0
  // local_.resize(len_);
  // synced_.resize(len_);
  local_ = EVec::Zero(len_);
  synced_ = EVec::Zero(len_);
  // store the keys
  for (size_t i = 0; i < len_; ++i)
    keys_[i] = i;
  Container::Init();
}

template<class V>
Status DenseVector<V>::GetLocalData(Mail *mail) {
  mail->flag().set_key_start(0);
  mail->flag().set_key_end(len_);
  mail->flag().set_key_cksum(keys_.raw().ComputeCksum());
  mail->set_keys(keys_.raw());
  XArray<V> val(len_);
  for (size_t i = 0; i < len_; i++) {
    if (mail->flag().push_delta()) {
      val[i] = local_[i] - synced_[i];
    } else {
      val[i] = local_[i];
    }
  }
  synced_ = local_;
  mail->set_vals(val.raw());
  return Status::OK();
}

template<class V>
Status DenseVector<V>::MergeRemoteData(const Mail& mail) {
  XArray<Key> keys(mail.keys());
  XArray<V> vals(mail.vals());
  CHECK_EQ(keys.size(), vals.size());

  for (size_t i = 0; i < keys.size(); ++i) {
    Key k = keys[i];
    V v = vals[i];
    if (mail.flag().push_delta()) {
      local_[k] += v;
      synced_[k] += v;
    } else {
      local_[k] += (v - synced_[k]);
      synced_[k] = v;
    }
  }
   LL << SName() << "merge: " << local_.norm() << " " << synced_.norm();
  return Status::OK();
}

}
