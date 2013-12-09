#pragma once

#include "box/container.h"
#include "util/xarray.h"

namespace PS {
// a single iterm container providing reduceall operator. it is usually used for
// communicating the algorithm progress such as objective value. the iterm
// should be sizeof-able, and provide the + operator

template <typename T>
class Item : public Container {
 public:
  explicit Item(const string& name);

  Status AllReduce(std::shared_future<T>* fut = NULL);
  const T& data() const { return data_; }
  T& data() { return data_; }

  Status GetLocalData(Mail *mail);
  Status MergeRemoteData(const Mail& mail);

  void Accept(const Mail& mail) {
    Container::Accept(mail);
    ReadAll();
  }
  // void Notify(const SyncFlag& flag) {
  //   consistency_.Response(flag, true);
  //   ReadAll();
  // }
 private:
  XArray<Key> keys_;
  T data_;
  std::map<time_t, T> merges_;
  FuturePool<T> fut_ret_;
};

template <typename T>
Item<T>::Item(const string& name)
    : Container(name), keys_(1) {
  keys_[0] = 0;
  Container::Init(KeyRange(0,1));
  if (IsRoot()) {
    aggregator_.SetDefaultType(NodeGroup::kAll);
  }
}

template <typename T>
Status Item<T>::MergeRemoteData(const Mail& mail) {
  // LL << my_node().ShortName() << "merge";
  // LL << mail.flag().DebugString();
  CHECK_EQ(mail.keys().size(), sizeof(Key));
  CHECK_EQ(mail.vals().size(), sizeof(T));
  T v;
  memcpy(&v, mail.vals().data(), sizeof(T));
  int32 t = mail.flag().time();
  if (aggregator_.ValidDefault()) {
    if (aggregator_.IsFirst(t)) {
      merges_[t] = v;
    } else {
      merges_[t] += v;
    }
    auto g = postmaster_->GetNodeGroup(name_);
    if (aggregator_.Success(t, g)) {
      data_ = merges_[t];
      fut_ret_.Set(t, data_);
    }
  } else {
    data_ = v;
    fut_ret_.Set(t,v);
  }
  return Status::OK();
}


template <typename T>
Status Item<T>::GetLocalData(Mail *mail) {
  // LL << my_node().ShortName() << "get";
  mail->flag().mutable_key()->set_start(0);
  mail->flag().mutable_key()->set_end(1);
  mail->flag().mutable_key()->set_cksum(keys_.raw().ComputeCksum());
  mail->set_keys(keys_.raw());
  size_t size = sizeof(data_);
  char* buff = new char[size];
  memcpy(buff, &data_, size);
  RawArray v(buff, size, 1);
  mail->set_vals(v);
  return Status::OK();
}

template <typename T>
Status Item<T>::AllReduce(std::shared_future<T>* fut) {
  Header flag;
  flag.set_type(Header::PUSH_PULL);
  flag.set_recver(postmaster_->GetNodeGroup(name_).root());
  Status s = Push(flag);
  fut_ret_.Insert(Clock(), fut);
  return s;
}

} // namespace PS
