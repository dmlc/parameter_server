#pragma once
#include "util/common.h"

namespace PS {

// an unique id presents a node or a group of node
typedef int32 uid_t;

// a computational node, can be both a server node or a client node
class Node {
 public:
  // the type of node
  static const int kTypeClient = 1;
  static const int kTypeServer = 0;
  // unique id of a node, always >=0
  static uid_t uid(int type, int id) { return id * 2 + type; }
  static uid_t GetUid(const string& type, int id);
  static bool Server(const string& type) {
    if (type == "server" || type == "Server" || type == "s")
      return true;
    return false;
  }
  static bool Client(const string& type) {
    if (type == "client" || type == "Client" || type == "c")
      return true;
    return false;
  }
  // constructor
  Node() {}
  Node(int type, int id, string net_addr, string cmd_addr = "") { Init(type, id, net_addr, cmd_addr); }
  void Init(int type, int id, string net_addr, string cmd_addr = "");

  bool is_client() const { return type_ == kTypeClient; }
  bool is_server() const { return type_ == kTypeServer; }

  int id() const { return id_; }
  uid_t uid() const { return uid_; }
  const string& addr() const { return net_addr_; };
  const string& cmd_addr() const { return cmd_addr_; }

  bool Valid () const ;
  string ToString() const;
  string ShortName() const;
 private:
  // a logic id, say client 0,1,2, server 0,1,2,3...
  int id_;
  // a unique id for each node, similar to rank_id in MPI
  uid_t uid_;
  // the physical network address, say tcp://192.168.1.2:8888, or inproc://server_1
  string net_addr_;
  string cmd_addr_;
  // whether this node is a client or a server
  int type_;
};

} // namespace PS
