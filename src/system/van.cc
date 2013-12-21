#include "system/van.h"
#include <string.h>
#include <zmq.h>
// #include "util/zmq.h"


namespace PS {

bool Van::Init() {
  context_ = zmq_ctx_new ();
  if (!context_) {
    LOG(ERROR) << "create 0mq context failed";
    return false;
  }
  receiver_ = NULL;
  return true;
}

bool Van::Destroy() {
  // TODO add some check
  zmq_close (receiver_);
  for (size_t i = 0; i < senders_.size(); ++i) {
    if (!senders_[i])
      zmq_close (senders_[i]);
  }
  zmq_ctx_destroy (context_);
  return true;
}

bool Van::Bind(Node const& node, int flag) {
  if (receiver_ != NULL) {
    LOG(ERROR) << "receiver already exists";
    return false;
  }
  receiver_ = zmq_socket(context_, ZMQ_ROUTER);
  if (receiver_ == NULL) {
    LOG(ERROR) << "create receiver socket failed: " << zmq_strerror(errno);
    return false;
  }
  // it is tricky, replace localhost or ip into *. so we extract the port from
  // the address:
  std::vector<string> part;
  if (!flag)
    part = split(node.addr(), ':');
  else
    part = split(node.cmd_addr(), ':');
  string address = "tcp://*:" + part.back();
  int rc = zmq_bind(receiver_, address.c_str());
  if (rc == -1) {
    LOG(ERROR) << "bind to " << address << " failed: " << zmq_strerror(errno);
    return false;
  }
#ifdef _DEBUG_VAN_
  LL << "node " << node.uid() << " binds address " << address;
#endif

  return true;
}

bool Van::Connect(Node const& node, int flag) {
  uid_t uid = node.uid();
  // uid_t ruid = - uid;
  int rc;
  if (senders_.find(uid) != senders_.end()) {
    LOG(ERROR) << "sender already exists";
    return false;
  }
  void *sender = zmq_socket(context_, ZMQ_DEALER);
  if (sender == NULL) goto CONNECT_FAIL;
  // TODO it doesn't work! instead, we set the identity in the protobuf
  // rc = zmq_setsockopt(sender, ZMQ_IDENTITY, &ruid, sizeof(ruid));
  // if (rc == -1) goto CONNECT_FAIL;

  if (!flag)
    rc = zmq_connect(sender, node.addr().c_str());
  else
    rc = zmq_connect(sender, node.cmd_addr().c_str());
  if (rc == -1) {
    LOG(ERROR) << "failed to connect to " << node.ToString();
    goto CONNECT_FAIL;
  }
  senders_[uid] = sender;
  return true;

CONNECT_FAIL:
  LOG(ERROR) << "zmq connection fail: " << zmq_strerror(errno);
  return false;
}

// TODO use zmq_msg_t to allow zero_copy send
Status Van::Send(const Mail& mail){

  // find the socket
  const Header& head = mail.flag();
  uid_t uid = (uid_t) head.recver();
  if (senders_.find(uid) == senders_.end()) {
    return Status::NotFound(StrCat("there is no socket to node ", uid));
  }
  // send the header
  void *socket = senders_[uid];
  string head_string;
  if (!head.SerializeToString(&head_string)) {
    return Status::InvalidArgument(StrCat("failed to serialize ",
                                          head.DebugString()));
  }
  int tag = ZMQ_SNDMORE;
  bool send_key = head.has_key() && !head.key().empty();
  bool send_value = head.has_value() && !head.value().empty();
  int n = send_key + send_value;
  if (n == 0) tag = 0;
  size_t rc = zmq_send(socket, head_string.c_str(), head_string.size(), tag);
  if (rc != head_string.size()) {
    return Status::NetError(StrCat("failed to send header to node ",
                                   uid,zmq_strerror(errno)));
  }
  // send keys
  if (send_key) {
    if ((--n) == 0) tag = 0;
    rc = zmq_send(socket, mail.keys().data(), mail.keys().size(), tag);
    if (rc != mail.keys().size()) {
      return Status::NetError(StrCat("failed to send keys to node ",
                                     uid, zmq_strerror(errno)));
    }
  }
  // send values
  if (send_value) {
    if ((--n) == 0) tag = 0;
    rc = zmq_send(socket, mail.vals().data(), mail.vals().size(), tag);
    if (rc != mail.vals().size()) {
      return Status::NetError(StrCat("failed to send values to node ",
                                     uid, zmq_strerror(errno)));
    }
  }
  CHECK_EQ(n, 0);
  CHECK_EQ(tag, 0);

#ifdef _DEBUG_VAN_
  LL << "time: " << mail.flag().time() << ", send to "
     << mail.flag().recver() << " "
     << mail.keys().size() << "(" << send_key <<  ") keys and "
     << mail.vals().size() << "(" << send_value << ") vals";
#endif

  return Status::OK();
}

Status Van::Recv(Mail *mail) {
  int n = 1;
  bool recv_key, recv_value;
  for (int i = 0; n > 0; ++i) {
    zmq_msg_t msg;
    int rc = zmq_msg_init(&msg);
    if (rc) {
      return Status::NetError("init msg fail");
    }
    rc = zmq_msg_recv(&msg, receiver_, 0);
    if (rc == -1) {
      return Status::NetError(StrCat("recv identity failed: ",
                                     zmq_strerror(errno)));
    }
    char* buf = (char *)zmq_msg_data(&msg);
    size_t size = zmq_msg_size(&msg);
    if (buf == NULL) {
      return Status::NetError(StrCat("get buff failed"));
    }
    switch (i) {
      case 0:
      // receive the identity
      // TODO didn't figure out how to get the id now.
      // *uid = - (uid_t) atoi(buf);
        break;
      case 1:
        {
          // it's the header
          string head_string(buf, size);
          if (!mail->flag().ParseFromString(head_string)) {
            return Status::NetError(StrCat("parse header fail"));
          }
          recv_key = mail->flag().has_key() && !mail->flag().key().empty();
          recv_value = mail->flag().has_value() && !mail->flag().value().empty();

          n = recv_key + recv_value;
          if (n && !recv_key) {
            // skip to receive key
            ++ i;
          }
        }
        break;
      case 2:
        {
          // receive keys
          char *data = (char*) malloc(size+5);
          memcpy(data, buf, size);
          mail->keys().Fill(data, 1, size);
          if ((--n) == 0) ++i;
        }
        break;

      case 3:
        {
          char *data = (char*) malloc(size+5);
          memcpy(data, buf, size);
          mail->vals().Fill(data, 1, size);
          --n;
        }
        break;
    }
    zmq_msg_close(&msg);
    if (n && !zmq_msg_more(&msg)) {
      return Status::NetError(StrCat("there should be more"));
    }
  }

#ifdef _DEBUG_VAN_
  LL << "time: " << mail->flag().time() << ", recv from "
     << mail->flag().sender() << " "
     << mail->keys().size() << "(" << recv_key <<  ") keys and "
     << mail->vals().size() << "(" << recv_value << ") vals";
#endif
  return Status::OK();;
}

// TODO use zmq_msg_t to allow zero_copy send
Status Van::Send(const NodeManagementInfo& mgt_info){
  // find the socket
  uid_t uid = (uid_t) mgt_info.recver();
  if (senders_.find(uid) == senders_.end()) {
    return Status::NotFound(StrCat("there is no socket to node ", uid));
  }
  // send nodemanagementinfo
  void *socket = senders_[uid];
  string mgt_info_string;
  if (!mgt_info.SerializeToString(&mgt_info_string)) {
    return Status::InvalidArgument(StrCat("failed to serialize ",
                                          mgt_info.DebugString()));
  }
  int tag = 0;
  size_t rc = zmq_send(socket, mgt_info_string.c_str(), mgt_info_string.size(), tag);
  if (rc != mgt_info_string.size()) {
    return Status::NetError(StrCat("failed to send syncflag to node ",
                                   uid,zmq_strerror(errno)));
  }
  return Status::OK();
}


Status Van::Recv(NodeManagementInfo *mgt_info, bool blocking) {
  zmq_msg_t msg;
  // receive identity
  int rc = zmq_msg_init(&msg);
  if (rc) {
    return Status::NetError("init msg fail");
  }

  if (!blocking)
    rc = zmq_msg_recv(&msg, receiver_, ZMQ_DONTWAIT);
  else
    rc = zmq_msg_recv(&msg, receiver_, 0);

  if (rc == -1) {
    return Status::NetError(StrCat("recv identity failed: ",zmq_strerror(errno)));
  }

  char* buf = (char *)zmq_msg_data(&msg);
  size_t size = zmq_msg_size(&msg);
  if (buf == NULL) {
    return Status::NetError(StrCat("get buff failed"));
  }

  // receive message
  rc = zmq_msg_init(&msg);
  if (rc) {
    return Status::NetError("init msg fail");
  }

  if (!blocking)
    rc = zmq_msg_recv(&msg, receiver_, ZMQ_DONTWAIT);
  else
    rc = zmq_msg_recv(&msg, receiver_, 0);

  if (rc == -1) {
    return Status::NetError(StrCat("recv nodemanagementinfo failed: ",zmq_strerror(errno)));
  }

  buf = (char *)zmq_msg_data(&msg);
  size = zmq_msg_size(&msg);
  if (buf == NULL) {
    return Status::NetError(StrCat("get buff failed"));
  }

  string mgt_string(buf, size);
  if (!mgt_info->ParseFromString(mgt_string)) {
    return Status::NetError(StrCat("parse nodemanagementinfo fail"));
  }

  // if (!zmq_msg_more(&msg)) {
  //   return Status::NetError(StrCat("there should be more"));
  // }
  zmq_msg_close(&msg);

  return Status::OK();;
}


} // namespace PS
