#pragma once
#include "system/customer.h"

// A simple interface for writing parameter server (PS) programs. see example in
// src/app/hello_world

// A typical PS program should define the following two functions:

// This is the main entrance for a work node. All flags and their arguments
// (e.g. -name value) has been removed from argv, and argc has been changed
// properly. However, commandline arguments are remained.
//
// In example "head -n 100 file", -n is a flag and 100 is this flag's argument,
// but file is a commandline argument
int WorkerNodeMain(int argc, char *argv[]);

namespace PS {

// Return an instance of a server node. This node is started with "-app_file
// app.conf -app_conf 'key: value'", then conf has both the content of file "app.conf"
// and 'key:value'
App* CreateServerNode(const std::string& conf);

// Utility functions:

// The app this node runs
inline App* MyApp() { return Postoffice::instance().manager().app(); }

// My node information
inline Node MyNode() { return Postoffice::instance().manager().van().my_node(); }
// Each unique string id of my node
inline std::string MyNodeID() { return MyNode().id(); }
// Query the role of this node
inline int IsWorker() { return MyNode().role() == Node::WORKER; }
inline int IsServer() { return MyNode().role() == Node::SERVER; }
inline int IsScheduler() { return MyNode().role() == Node::SCHEDULER; }

inline Range<Key> MyKeyRange() { return Range<Key>(MyNode().key()); }
inline std::string SchedulerID() {
  return Postoffice::instance().manager().van().scheduler().id();
}

inline int NextCustomerID() {
  return Postoffice::instance().manager().NextCustomerID();
}

// The rank ID of this node in its group. Assume this a worker node in a worker
// group with N workers. Then this node will be assigned an unique ID from 0,
// ..., N. Similarly for server and scheduler.
inline int MyRank() { return MyNode().rank(); }
// Total nodes in this node group.
inline int RankSize() {
  auto& mng = Postoffice::instance().manager();
  return IsWorker() ? mng.num_workers() : (IsServer() ? mng.num_servers() : 1);
}

// Wait until all FLAGS_num_servers servers are ready.
inline void WaitServersReady() {
  PS::Postoffice::instance().manager().WaitServersReady();
}

// Wait until all FLAGS_num_workers workers are ready.
inline void WaitWorkersReady() {
  PS::Postoffice::instance().manager().WaitWorkersReady();
}

inline void StartSystem(int argc, char *argv[]) {
  PS::Postoffice::instance().Run(&argc, &argv);
}

inline void StopSystem() {
  PS::Postoffice::instance().Stop();
}

inline int RunSystem(int argc, char *argv[]) {
  StartSystem(argc, argv); StopSystem();
  return 0;
}

} // namespace PS
