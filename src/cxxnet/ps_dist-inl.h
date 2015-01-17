#pragma once
#include "mshadow-ps/ps.h"
#include "cxxnet/kv_array.h"
namespace mshadow {
namespace ps {

template<typename xpu, typename DType>
class DistServer : public IParamServer<xpu, DType>  {
 public:
  virtual void SetParam(const char *name, const char *val) {
    if (!strcmp(name, "name")) {
      name_ = std::string(val);
    } else if (!strcmp(name, "parent_name")) {
      parent_name_ = std::string(val);
    }
  }

  virtual void Init(const std::vector<int> &devices) {
    PS::CHECK_EQ(devices.size(), 1);
    PS::CHECK(!name_.empty());
    PS::CHECK(!parent_name_.empty());
    shared_model_ = new PS::KVVector<DType>(name_, parent_name_);
  }


  virtual void PullWait(int key, int devid) {
    using namespace PS;
    auto it = last_pull_.find(key);
    if (it == last_pull_.end() || it->second == -1) return;
    CHECK_NOTNULL(shared_model_)->waitOutMsg(kServerGroup, it->second);
    it->second = -1;
  }
 protected:

  virtual void Push_(
      Tensor<xpu, 2, DType> data, int key, int devid, int priority) {
    using namespace PS;
    // TODO the zero copy version
    // SArray<DType> val(data.dptr_, data.MSize(), false);
    SArray<DType> val; val.copyFrom(data.dptr_, data.MSize());

    Message msg(new Message(kServerGroup));
    msg->addValue(val);
    msg->task.set_key_channel(key);
    Range<Key>(0, val.size()).to(msg->task.mutable_key_range());
    CHECK_NOTNULL(shared_model_)->push(push);
  }

  virtual void PullReq_(
      Tensor<xpu, 2, DType> data, int key, int devid, int priority,
      CallbackFunction callback, void *callback_arg) {
    using namespace PS;

    Message msg(new Message(kServerGroup));
    msg->task.set_key_channel(key);
    Range<Key>(0, val.size()).to(msg->task.mutable_key_range());
    last_pull_[key] = CHECK_NOTNULL(shared_model_)->pull(push);
  }

 protected:
  unordered_map<int, int> last_pull_;
  std::string name_;
  std::string parent_name_;
  PS::KVVector<DType>* shared_model_;
};
} // namespace ps
} // namespace mshadow
