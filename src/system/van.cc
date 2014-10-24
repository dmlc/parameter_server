#include "system/van.h"
#include <string.h>
#include <zmq.h>
#include "base/shared_array_inl.h"
#include "util/local_machine.h"

namespace PS {

DEFINE_string(my_node, "role:SCHEDULER,hostname:'127.0.0.1',port:8000,id:'H'", "my node");
DEFINE_string(scheduler, "role:SCHEDULER,hostname:'127.0.0.1',port:8000,id:'H'", "the scheduler node");
DEFINE_string(server_master, "", "the master of servers");
DEFINE_bool(print_van, false, "");
DEFINE_int32(my_rank, -1, "my rank among MPI peers");
DEFINE_string(interface, "", "network interface");

DECLARE_int32(num_workers);
DECLARE_int32(num_servers);

void Van::init() {
  scheduler_ = parseNode(FLAGS_scheduler);
  if (FLAGS_my_rank < 0) {
    my_node_ = parseNode(FLAGS_my_node);
  } else {
    my_node_ = assembleMyNode();
  }
  // LI << "I am [" << my_node_.ShortDebugString() << "]; pid:" << getpid();

  context_ = zmq_ctx_new();
  // TODO the following does not work...
  // zmq_ctx_set(context_, ZMQ_MAX_SOCKETS, 1000000);
  // zmq_ctx_set(context_, ZMQ_IO_THREADS, 4);
  // LL << "ZMQ_MAX_SOCKETS: " << zmq_ctx_get(context_, ZMQ_MAX_SOCKETS);

  CHECK(context_ != NULL) << "create 0mq context failed";
  bind();
  connect(my_node_);
  connect(scheduler_);

  if (FLAGS_print_van) {
    debug_out_.open("van_"+my_node_.id());
  }
}

void Van::destroy() {
  for (auto& it : senders_) zmq_close (it.second);
  zmq_close (receiver_);
  zmq_ctx_destroy (context_);
}

void Van::bind() {
  receiver_ = zmq_socket(context_, ZMQ_ROUTER);
  CHECK(receiver_ != NULL)
      << "create receiver socket failed: " << zmq_strerror(errno);
  CHECK(my_node_.has_port()) << my_node_.ShortDebugString();
  string addr = "tcp://*:" + std::to_string(my_node_.port());
  // string addr = "tcp://" + address(my_node_);
  CHECK(zmq_bind(receiver_, addr.c_str()) == 0)
      << "bind to " << addr << " failed: " << zmq_strerror(errno);

  if (FLAGS_print_van) {
    debug_out_ << my_node_.id() << ": binds address " << addr << std::endl;
  }
}

Status Van::connect(const Node& node) {
  CHECK(node.has_id()) << node.ShortDebugString();
  CHECK(node.has_port()) << node.ShortDebugString();
  CHECK(node.has_hostname()) << node.ShortDebugString();
  NodeID id = node.id();
  // the socket already exists? probably we are re-connecting to this node
  if (senders_.find(id) != senders_.end()) {
    // zmq_close (senders_[id]);
    return Status::OK();
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
  if (zmq_connect(sender, addr.c_str()) != 0)
    return Status:: NetError(
        "connect to " + addr + " failed: " + zmq_strerror(errno));
  senders_[id] = sender;

  if (FLAGS_print_van) {
    debug_out_ << my_node_.id() << ": connect to " << addr << std::endl;
  }
  return Status::OK();
}

// TODO use zmq_msg_t to allow zero_copy send
// btw, it is not thread safe
Status Van::send(const MessagePtr& msg, size_t* send_bytes) {

  // find the socket
  NodeID id = msg->recver;
  auto it = senders_.find(id);
  if (it == senders_.end())
    return Status::NotFound("there is no socket to node " + (id));
  void *socket = it->second;

  // double check
  bool has_key = !msg->key.empty();
  msg->task.set_has_key(has_key);
  int n = has_key + msg->value.size();

  // send task
  string str;
  CHECK(msg->task.SerializeToString(&str))
      << "failed to serialize " << msg->task.ShortDebugString();
  int tag = ZMQ_SNDMORE;
  if (n == 0) tag = 0; // ZMQ_DONTWAIT;
  while (true) {
    if (zmq_send(socket, str.c_str(), str.size(), tag) == str.size()) break;
    if (errno == EINTR) continue;  // may be interupted by google profiler
    return Status::NetError(
        "failed to send mailer to node " + (id) + zmq_strerror(errno));
  }
  *send_bytes += str.size();
  data_sent_ += str.size();

  // send data
  for (int i = 0; i < n; ++i) {
    const auto& raw = (has_key && i == 0) ? msg->key : msg->value[i-has_key];
    if (i == n - 1) tag = 0; // ZMQ_DONTWAIT;
    while (true) {
      if (zmq_send(socket, raw.data(), raw.size(), tag) == raw.size()) break;
      if (errno == EINTR) continue;  // may be interupted by google profiler
      return Status::NetError(
          "failed to send mailer to node " + (id) + zmq_strerror(errno));
    }
    *send_bytes += raw.size();
    data_sent_ += raw.size();
  }

  if (FLAGS_print_van) {
    debug_out_ << "|>>>   " << msg->shortDebugString()<< std::endl;
  }
  return Status::OK();
}

// TODO Zero copy
Status Van::recv(const MessagePtr& msg, size_t* recv_bytes) {
  msg->clearData();
  NodeID sender;
  for (int i = 0; ; ++i) {
    zmq_msg_t zmsg;
    CHECK(zmq_msg_init(&zmsg) == 0) << zmq_strerror(errno);
    while (true) {
      if (zmq_msg_recv(&zmsg, receiver_, 0) != -1) break;
      if (errno == EINTR) continue;  // may be interupted by google profiler
      return Status::NetError(
          "recv message failed: " + std::string(zmq_strerror(errno)));
    }
    char* buf = (char *)zmq_msg_data(&zmsg);
    CHECK(buf != NULL);
    size_t size = zmq_msg_size(&zmsg);
    *recv_bytes += size;
    data_received_ += size;
    if (i == 0) {
      // identify
      sender = id(std::string(buf, size));
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

  if (FLAGS_print_van) {
    debug_out_ << "|<<<   " << msg->shortDebugString() << std::endl;
  }
  return Status::OK();;
}

void Van::statistic() {
  if (my_node_.role() == Node::UNUSED || my_node_.role() == Node::SCHEDULER) return;
  auto gb = [](size_t x) { return  x / 1e9; };
  LI << my_node_.id() << " sent " << gb(data_sent_)
     << " Gbyte, received " << gb(data_received_) << " Gbyte";
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
  unsigned short port;
  if (FLAGS_interface.empty()) {
    LocalMachine::pickupAvailableInterfaceAndIP(FLAGS_interface, ip);
  } else {
    LocalMachine::IP(FLAGS_interface);
  }
  if (ip.empty() || FLAGS_interface.empty()) {
    throw std::runtime_error("got interface/ip failed");
  }
  port = LocalMachine::pickupAvailablePort();
  if (0 == port) {
    throw std::runtime_error("got port failed");
  }
  ret_node.set_hostname(ip);
  ret_node.set_port(static_cast<int32>(port));

  return ret_node;
}

bool Van::connected(const Node& node) {
  auto it = senders_.find(node.id());
  return it != senders_.end();
}


} // namespace PS
