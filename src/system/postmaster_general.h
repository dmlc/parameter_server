#include "util/common.h"

namespace PS {

class NameID {
 public:
  NameID() : available_id_(0) { }
  int32 GetID(string name) {
    for (auto& i : id2name_) {
      if (i.second == name) {
        return i.first;
      }
    }
    auto id = available_id_ ++;
    id2name_[id] = name;
    return id;
  }
  string GetName(int32 id) {
    return id2name_[id];
  }
 private:
  map<int32, string> id2name_;
  int32 available_id_;
};

// A Postmaster General is the chief executive officer of the postal service of
// a country, responsible for oversight over all other Postmasters. PG is unique
// and exists at the master node.
// TODO one may consider zonekeeper
class PostmasterGeneral {

  void ProcessExpress(const Express& cmd);
 private:

  Postoffice* postoffice_;

  AddressBook addr_book_;

  map<int32, NodeGroup> obj_nodegroups_;
  map<pair<int32, uid_t>, Workload> obj_workloads_;

  NameID name_id_;
};
} // namespace PS
