#pragma once
#include "system/postoffice.h"
#include "system/app.h"

// A simple interface to write parameter server (PS) programs. see example in
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

// Utility functions.

// The app this node runs
inline App* MyApp() { return PS::Postoffice::instance().app(); }

inline Node MyNode() { return PS::Postoffice::instance().myNode(); }

// Each node has an unique string id.
inline std::string MyNodeID() { return MyNode().id(); }

// query the role of this node
inline int IsWorker() { return MyNode().role() == Node::WORKER; }
inline int IsServer() { return MyNode().role() == Node::SERVER; }
inline int IsScheduler() { return MyNode().role() == Node::SCHEDULER; }


// The rank ID of this node in its group. Assume this a worker node in a worker
// group with N workers. Then this node will be assigned an unique ID from 0,
// ..., N. Similarly for server and scheduler.
inline int MyRank() { return MyApp()->myRank(); }

// Total nodes in this node group.
inline int RankSize() {
  auto& yp = PS::Postoffice::instance().yp();
  return IsWorker() ? yp.num_workers() : (IsServer() ? yp.num_servers() : 1);
}

} // namespace PS
