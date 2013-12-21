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
  // set the default type
  void SetDefaultType(uid_t default_type) { default_type_ = default_type; }
  // you can set different type for different time, it will overide the default
  void SetType(int32 time, uid_t type) { type_[time] = type; }
  inline uid_t GetType(int32 time);

  // whether or not there is a default valid aggregator
  bool ValidDefault() {
    return NodeGroup::Valid(default_type_);
  }
  // whether or not there is a valid aggregator on time time
  bool Valid(int32 time) {
    return NodeGroup::Valid(GetType(time));
  }

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
  map<int32, uid_t> type_;
  // key: timestampe, value: a set of nodes from which we have received
  // data at this timestamp. we also keep the received flags in case pushing
  // back is required
  map<int32, map<uid_t, Mail> > status_;
};

uid_t Aggregator::GetType(int32 time) {
  uid_t type = default_type_;
  if (type_.find(time) != type_.end())
    type = type_[time];
  return type;
}

bool Aggregator::Success(int32 time, const NodeGroup& group) {
  // get the status
  auto it = status_.find(time);
  if (it == status_.end())
    return false;
  auto& st = it->second;
  // get the type
  uid_t type = GetType(time);
  if (!NodeGroup::Valid(type)) {
    CHECK_EQ(st.size(), (size_t)1) << "only one mail is expected from node "
                           << type << " at time " << time;
    return true;
  }
  // check if get the results from all expected nodes
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
