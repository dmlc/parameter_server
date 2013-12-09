#pragma once
#include "box/container.h"
#include "util/xarray.h"
namespace PS {
// synchronize an arbitrary class, such as the progress of the learning
// algorithm, or running statistics
// the class should have a method to serialize itself:
//    SerializeToString(string *content)
//    ParseFromString(string& content)
// which is default provided by protobuf. if you want to use AllReduce, your
// class then need to provide the + operator
template <typename T>
class Metadata : public Container {
 public:
  explicit Metadata(const string& name);
  Status AllReduce(Future* fut) { }
  Status PushToRoot(Future *fut) { }

  T& data() { return data_; }

  Status GetLocalData(Mail *mail);
  Status MergeRemoteData(const Mail& mail);
 private:
  XArray<Key> keys_;
  T data_;
};

template <typename T>
Metadata<T>::Metadata(const string& name)
    : Container(name, 0, 1), keys_(1) {
  keys_[0] = 0;
}

template <typename T>
Status Metadata<T>::MergeRemoteData(const Mail& mail) {
  CHECK_EQ(mail.keys().size(), sizeof(Key));
  // CHECK_EQ(mail.values().entry_num(), 1);
  string str(mail.values().data(), mail.values().size());
  if (!data_.ParseFromString(str)) {
    return Status::InvalidArgument(StrCat("failed to parse data"));
  }
  return Status::OK();
}

template <typename T>
Status Metadata<T>::GetLocalData(Mail *mail) {
  mail->set_keys(keys_.raw());
  string str;
  if (!data_.SerializeToString(&str)) {
    return Status::InvalidArgument(StrCat("failed to serialize data"));
  }
  RawArray v(str.data(), str.size(), 1);
  mail->set_values(v);
  return Status::OK();
}


} // namespace PS
