#pragma once
#include "util/common.h"
#include "util/mail.h"
#include "util/status.h"
#include "system/node.h"
#include "proto/nodemgt.pb.h"

// #define _DEBUG_VAN_

// serialize message
//#include <google/protobuf/message_lite.h>
namespace PS {

// Van is a wrapper of zeromq, it sends and receives a flag with a updt to a
// node.
class Van {
 public:
  // SINGLETON(Van);

  Van() {}
  bool Init();
  bool Destroy();

  bool Bind(Node const& node, int flag = 0);
  bool Connect(Node const& node, int flag = 0);

  Status Send(const Mail& mail);
  Status Recv(Mail* mail);

  Status Send(const NodeManagementInfo& mgt_info);
  Status Recv(NodeManagementInfo* mgt_info, bool blocking = true);


 private:
  // DISALLOW_COPY_AND_ASSIGN(Van);
  void *context_;
  void *receiver_;
  std::map<uid_t, void *> senders_;
};

} // namespace PS
