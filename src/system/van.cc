#include "system/van.h"
#include <string.h>
#include <zmq.h>
#include "util/shared_array_inl.h"
#include "util/local_machine.h"
namespace PS {

DEFINE_string(my_node, "role:SCHEDULER,hostname:'127.0.0.1',port:8000,id:'H'", "my node");
DEFINE_string(scheduler, "role:SCHEDULER,hostname:'127.0.0.1',port:8000,id:'H'", "the scheduler node");
// DEFINE_bool(print_van, false, "");
DEFINE_int32(bind_to, 0, "binding port");
DEFINE_int32(my_rank, -1, "my rank among MPI peers");
DEFINE_string(interface, "", "network interface");

DECLARE_int32(num_workers);
DECLARE_int32(num_servers);

Van::~Van() {
  statistic();
  for (auto& it : senders_) zmq_close (it.second);
  zmq_close (receiver_);
  zmq_ctx_destroy (context_);
}

void Van::init(char* argv0) {
  scheduler_ = parseNode(FLAGS_scheduler);
  if (FLAGS_my_rank < 0) {
    my_node_ = parseNode(FLAGS_my_node);
  } else {
    my_node_ = assembleMyNode();
  }

  if (FLAGS_log_dir.empty()) FLAGS_log_dir = "/tmp";
  // change the hostname in default log filename to node id
  string logfile = FLAGS_log_dir + "/" + string(basename(argv0))
                   + "." + my_node_.id() + ".log.";
  google::SetLogDestination(google::INFO, (logfile+"INFO.").c_str());
  google::SetLogDestination(google::WARNING, (logfile+"WARNING.").c_str());
  google::SetLogDestination(google::ERROR, (logfile+"ERROR.").c_str());
  google::SetLogDestination(google::FATAL, (logfile+"FATAL.").c_str());
  google::SetLogSymlink(google::INFO, "");
  google::SetLogSymlink(google::WARNING, "");
  google::SetLogSymlink(google::ERROR, "");
  google::SetLogSymlink(google::FATAL, "");
  FLAGS_logbuflevel = -1;

  context_ = zmq_ctx_new();
  CHECK(context_ != NULL) << "create 0mq context failed";

  // one need to "sudo ulimit -n 65536" or edit /etc/security/limits.conf
  zmq_ctx_set(context_, ZMQ_MAX_SOCKETS, 65536);
  // zmq_ctx_set(context_, ZMQ_IO_THREADS, 4);

  bind();
  connect(my_node_);
  connect(scheduler_);
}


void Van::bind() {
  receiver_ = zmq_socket(context_, ZMQ_ROUTER);
  CHECK(receiver_ != NULL)
      << "create receiver socket failed: " << zmq_strerror(errno);
  string addr = "tcp://*:";
  if (FLAGS_bind_to) {
    addr += std::to_string(FLAGS_bind_to);
  } else {
    CHECK(my_node_.has_port()) << my_node_.ShortDebugString();
    addr += std::to_string(my_node_.port());
  }
  CHECK(zmq_bind(receiver_, addr.c_str()) == 0)
      << "bind to " << addr << " failed: " << zmq_strerror(errno);

  VLOG(1) << "BIND address " << addr;
}

void Van::disconnect(const Node& node) {
  CHECK(node.has_id()) << node.ShortDebugString();
  NodeID id = node.id();
  if (senders_.find(id) != senders_.end()) {
    zmq_close (senders_[id]);
  }
  senders_.erase(id);
  VLOG(1) << "DISCONNECT from " << node.id();
}

bool Van::connect(const Node& node) {
  CHECK(node.has_id()) << node.ShortDebugString();
  CHECK(node.has_port()) << node.ShortDebugString();
  CHECK(node.has_hostname()) << node.ShortDebugString();
  NodeID id = node.id();
  if (senders_.find(id) != senders_.end()) {
    VLOG(1) << "already connected to " << id;
    return true;
  }
  void *sender = zmq_socket(context_, ZMQ_DEALER);
  CHECK(sender != NULL) << zmq_strerror(errno);
  string my_id = my_node_.id(); // address(my_node_);
  zmq_setsockopt (sender, ZMQ_IDENTITY, my_id.data(), my_id.size());

  // TODO is it useful?
  // uint64_t hwm = 5000000;
  // zmq_setsockopt (sender, ZMQ_SNDHWM, &hwm, sizeof(hwm));

  // connect
  string addr = "tcp://" + address(node);
  if (zmq_connect(sender, addr.c_str()) != 0) {
    LOG(WARNING) << "connect to " + addr + " failed: " + zmq_strerror(errno);
    return false;
  }

  senders_[id] = sender;
  hostnames_[id] = node.hostname();

  VLOG(1) << "CONNECT to " << id << " [" << addr << "]";
  return true;
}

// TODO use zmq_msg_t to allow zero_copy send
// btw, it is not thread safe
bool Van::send(const MessagePtr& msg, size_t* send_bytes) {
  // find the socket
  NodeID id = msg->recver;
  auto it = senders_.find(id);
  if (it == senders_.end()) {
    LOG(WARNING) << "there is no socket to node " + id;
    return false;
  }
  void *socket = it->second;

  // double check
  bool has_key = !msg->key.empty();
  if (has_key) {
    msg->task.set_has_key(has_key);
  } else {
    msg->task.clear_has_key();
  }
  int n = has_key + msg->value.size();

  // send task
  size_t data_size = 0;
  string str;
  CHECK(msg->task.SerializeToString(&str))
      << "failed to serialize " << msg->task.ShortDebugString();
  int tag = ZMQ_SNDMORE;
  if (n == 0) tag = 0; // ZMQ_DONTWAIT;
  while (true) {
    if (zmq_send(socket, str.c_str(), str.size(), tag) == str.size()) break;
    if (errno == EINTR) continue;  // may be interupted by google profiler
    LOG(WARNING) << "failed to send message to node [" << id
                 << "] errno: " << zmq_strerror(errno);
    return false;
  }
  data_size += str.size();

  // send data
  for (int i = 0; i < n; ++i) {
    const auto& raw = (has_key && i == 0) ? msg->key : msg->value[i-has_key];
    if (i == n - 1) tag = 0; // ZMQ_DONTWAIT;
    while (true) {
      if (zmq_send(socket, raw.data(), raw.size(), tag) == raw.size()) break;
      if (errno == EINTR) continue;  // may be interupted by google profiler
      LOG(WARNING) << "failed to send message to node [" << id
                   << "] errno: " << zmq_strerror(errno);
      return false;
    }
    data_size += raw.size();
  }

  // statistics
  *send_bytes += data_size;
  if (hostnames_[id] == my_node_.hostname()) {
    sent_to_local_ += data_size;
  } else {
    sent_to_others_ += data_size;
  }
  VLOG(1) << "TO " << msg->recver << " " << msg->shortDebugString();
  return true;
}

// TODO Zero copy
bool Van::recv(const MessagePtr& msg, size_t* recv_bytes) {
  size_t data_size = 0;
  msg->clearData();
  NodeID sender;
  for (int i = 0; ; ++i) {
    zmq_msg_t zmsg;
    CHECK(zmq_msg_init(&zmsg) == 0) << zmq_strerror(errno);
    while (true) {
      if (zmq_msg_recv(&zmsg, receiver_, 0) != -1) break;
      if (errno == EINTR) continue;  // may be interupted by google profiler
      LOG(WARNING) << "failed to receive message. errno: "
                   << zmq_strerror(errno);
      return false;
   }
    char* buf = (char *)zmq_msg_data(&zmsg);
    CHECK(buf != NULL);
    size_t size = zmq_msg_size(&zmsg);
    data_size += size;
    if (i == 0) {
      // identify
      sender = std::string(buf, size);
      msg->sender = sender;
      msg->recver = my_node_.id();
    } else if (i == 1) {
      // task
      CHECK(msg->task.ParseFromString(std::string(buf, size)))
          << "parse string from " << sender << " I'm " << my_node_.id() << " "
          << size;
    } else {
      // data
      SArray<char> data; data.copyFrom(buf, size);
      if (i == 2 && msg->task.has_key()) {
        msg->key = data;
      } else {
        msg->value.push_back(data);
      }
    }
    zmq_msg_close(&zmsg);
    if (!zmq_msg_more(&zmsg)) { CHECK_GT(i, 0); break; }
  }

  *recv_bytes += data_size;
  if (hostnames_[sender] == my_node_.hostname()) {
    received_from_local_ += data_size;
  } else {
    received_from_others_ += data_size;
  }
  VLOG(1) << "FROM: " << msg->sender << " " << msg->shortDebugString();
  return true;
}

void Van::statistic() {
  // if (my_node_.role() == Node::UNUSED || my_node_.role() == Node::SCHEDULER) return;
  auto gb = [](size_t x) { return  x / 1e9; };
  LOG(INFO) << my_node_.id()
            << " sent " << gb(sent_to_local_ + sent_to_others_)
            << " (local " << gb(sent_to_local_) << ") Gbyte,"
            << " received " << gb(received_from_local_ + received_from_others_)
            << " (local " << gb(received_from_local_) << ") Gbyte";
}

Node Van::assembleMyNode() {
  if (0 == FLAGS_my_rank) {
    return scheduler_;
  }

  Node ret_node;
  // role and id
  if (FLAGS_my_rank <= FLAGS_num_workers) {
    ret_node.set_role(Node::WORKER);
    ret_node.set_id("W" + std::to_string(FLAGS_my_rank - 1));
  } else if (FLAGS_my_rank <= FLAGS_num_workers + FLAGS_num_servers) {
    ret_node.set_role(Node::SERVER);
    ret_node.set_id("S" + std::to_string(FLAGS_my_rank - FLAGS_num_workers - 1));
  } else {
    ret_node.set_role(Node::UNUSED);
    ret_node.set_id("U" + std::to_string(
      FLAGS_my_rank - FLAGS_num_workers - FLAGS_num_servers - 1));
  }

  // IP, port and interface
  string ip;
  string interface = FLAGS_interface;
  unsigned short port;

  if (interface.empty()) {
    LocalMachine::pickupAvailableInterfaceAndIP(interface, ip);
  } else {
    ip = LocalMachine::IP(interface);
  }
  CHECK(!ip.empty()) << "failed to got ip";
  CHECK(!interface.empty()) << "failed to got the interface";
  port = LocalMachine::pickupAvailablePort();
  CHECK_NE(port, 0) << "failed to get port";
  ret_node.set_hostname(ip);
  ret_node.set_port(static_cast<int32>(port));

  return ret_node;
}

bool Van::connected(const Node& node) {
  auto it = senders_.find(node.id());
  return it != senders_.end();
}


} // namespace PS
