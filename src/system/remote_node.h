#pragma once
#include "util/common.h"
#include "system/proto/task.pb.h"
#include "system/van.h"
#include "system/postoffice.h"
#include "filter/filter.h"
namespace PS {

// The presentation of a remote node used by Executor. It's not thread
// safe, do not use them directly.

// Track a request by its timestamp.
class RequestTracker {
 public:
  RequestTracker() { }
  ~RequestTracker() { }

  // Returns true if timestamp "ts" is marked as finished.
  bool IsFinished(int ts) {
    return (data_.size() > ts) && data_[ts];
  }

  // Mark time timstamp "ts" as finished.
  void Finish(int ts) {
    CHECK_LT(ts, 1000000);
    if (data_.size() <= ts) data_.resize(ts*2+5);
    data_[ts] = true;
  }
 private:
  std::vector<bool> data_;
};

// A remote node
struct RemoteNode {
 public:
  RemoteNode() { }
  ~RemoteNode() {
    for (auto f : filters) delete f.second;
  }

  void EncodeMessage(const MessagePtr& msg);
  void DecodeMessage(const MessagePtr& msg);

  Node rnode;                               // the remote node
  bool alive = true;                        // aliveness
  std::condition_variable cond;

  // -- info of requests sent to "rnode" --
  RequestTracker sent_req_tracker;

  // -- info of request received from "rnode" --
  RequestTracker recv_req_tracker;

  // node group
  void AddSubNode(RemoteNode* rnode);
  void RemoveSubNode(RemoteNode* rnode);
  std::vector<RemoteNode*> nodes;
  KeyRangeList keys;

 private:
  Filter* FindFilterOrCreate(const FilterConfig& conf);
  // key: filter_type
  std::unordered_map<int, Filter*> filters;

};


} // namespace PS
