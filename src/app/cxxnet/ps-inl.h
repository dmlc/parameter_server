#pragma once
#include "mshadow-ps/ps.h"
#include "cxxnet/kv_array.h"
namespace mshadow {
namespace ps {

template<typename xpu, typename DType>
class ParamServer: public ISharedModel<xpu, DType>  {
 public:
  typedef typename ISharedModel<xpu, DType>::CallbackFunction CallbackFunction;
  virtual void SetParam(const char *name, const char *val) {
    if (!strcmp(name, "name")) {
      name_ = std::string(val);
    } else if (!strcmp(name, "parent_name")) {
      parent_name_ = std::string(val);
    }
  }

  virtual void Init(const std::vector<int> &devices) {
    CHECK_EQ(devices.size(), 1);
    CHECK(!name_.empty());
    CHECK(!parent_name_.empty());
    shared_model_ = new PS::KVArray<DType>(name_, parent_name_);
  }

  virtual void PullWait(int key, int devid) {
    using namespace PS;
    auto& rec = records_[key];
    if (rec.pull == -1) return;
    CHECK_NOTNULL(shared_model_)->waitOutMsg(kServerGroup, rec.pull);
    rec.pull = -1;
  }
 protected:

  virtual void Push_(
      Tensor<xpu, 2, DType> data, int key, int devid, int priority) {
    using namespace PS;
    // TODO the zero copy version
    // SArray<DType> val(data.dptr_, data.MSize(), false);
    SArray<DType> val; val.copyFrom(data.dptr_, data.MSize());

    MessagePtr msg(new Message(kServerGroup));
    msg->addValue(val);
    msg->task.set_key_channel(key);
    Range<Key>(0, val.size()).to(msg->task.mutable_key_range());
    records_[key].push = CHECK_NOTNULL(shared_model_)->push(msg);
  }

  virtual void PullReq_(
      Tensor<xpu, 2, DType> data, int key, int devid, int priority,
      CallbackFunction callback, void *callback_arg) {
    using namespace PS;

    // add an dependency on the prevous push, so we can do data consistency control
    auto& rec = records_[key];
    MessagePtr msg(new Message(kServerGroup, -1, rec.push));
    msg->task.set_key_channel(key);
    Range<Key>(0, data.MSize()).to(msg->task.mutable_key_range());
    msg->fin_handle = [this, callback, callback_arg, data, key]() {
      // copy data..
      const auto& recv = shared_model_->array(key);
      CHECK_EQ(data.MSize(), recv.size());
      memcpy(CHECK_NOTNULL(data.dptr_), recv.data(), recv.size()*sizeof(DType));
      // call callback, TODO
      if (callback) callback(nullptr, callback_arg);
    };
    rec.pull = CHECK_NOTNULL(shared_model_)->pull(msg);
  }

 protected:
  struct Record {
    int push = -1;
    int pull = -1;
    DType* data = nullptr;
  };
  std::unordered_map<int, Record> records_;

  std::string name_;
  std::string parent_name_;
  PS::KVArray<DType>* shared_model_;
};
} // namespace ps
} // namespace mshadow
