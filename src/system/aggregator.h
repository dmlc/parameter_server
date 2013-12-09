#pragma once

#include "util/common.h"
#include "util/mail.h"
#include "system/node_group.h"

namespace PS {
// not thread safe,
class Aggregator {
 public:
  Aggregator() : default_type_(NodeGroup::kInvalid) { }
  ~Aggregator() { }
  void SetDefaultType(uid_t default_type) { default_type_ = default_type; }
  bool ValidDefault() { return NodeGroup::Valid(default_type_); }
  // you can set different type for different time, it will overide
  void SetType(int32 time, uid_t type) { type_[time] = type; }

  // whether the type_ is valid.
  // bool Valid() const;
  // return true if the nodes from which we received data at time t match the
  // expected node group
  inline bool Success(int32 time, const NodeGroup& group);

  void Insert(const Mail& mail) {
    status_[mail.flag().time()][mail.flag().sender()] = mail;
  }
  void Delete(int32 t) { status_.erase(t); }
  // whether or not exact one item is received
  bool IsFirst(int32 t) { return (status_[t].size() == 1); }
  map<uid_t, Mail>& GetTime(int32 t) { return status_[t]; }
 private:
  uid_t default_type_;
  map<time_t, uid_t> type_;
  // key: timestampe, value: a set of nodes from which we have received
  // data at this timestamp. we also keep the received flags in case pushing
  // back is required
  map<time_t, map<uid_t, Mail> > status_;
};

bool Aggregator::Success(int32 time, const NodeGroup& group) {
  auto it = status_.find(time);
  if (it == status_.end())
    return false;

  uid_t type = default_type_;
  if (type_.find(time) != type_.end())
    type = type_[time];

  auto& st = it->second;
  if (!NodeGroup::Valid(type)) {
    CHECK_EQ(st.size(), 1) << "I expect receive only one from " << type;
    return true;
  }

  const NodeList& nodes = group.Get(type);
  if (nodes->size() > st.size())
    return false;
  for (auto id : *nodes) {
    if (st.find(id) == st.end())
      return false;
  }
  return true;
}


} // namespace PS
