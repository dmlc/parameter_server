#pragma once
#include "system/message.h"
#include "system/proto/heartbeat.pb.h"

namespace PS {

struct NodeIDCmp {
  void splitNodeID(const NodeID& in, string& primary, string& secondary) const;
  bool operator()(const NodeID& a, const NodeID& b) const;
};

class Dashboard {
 public:
  void addTask(const NodeID& node, int task_id);
  void addReport(const NodeID& node, const string& report);
  string report();
 private:
  string title();
  string report(const NodeID& node, const HeartbeatReport& report);
  std::mutex mu_;
  std::map<NodeID, HeartbeatReport, NodeIDCmp> data_;
};

} // namespace PS
