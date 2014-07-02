#include "system/van.h"
#include <string.h>
#include <zmq.h>
#include "base/shared_array_inl.h"

namespace PS {
// #define _DEBUG_VAN_
// static string van_filter = "";

DEFINE_string(my_node, "", "my node");
DEFINE_bool(compress_message, true, "");

void Van::init() {
  my_node_ = parseNode(FLAGS_my_node);
  context_ = zmq_ctx_new ();
  // TODO is it useful?
  // zmq_ctx_set(context_, ZMQ_MAX_SOCKETS, 1000000);
  // zmq_ctx_set(context_, ZMQ_IO_THREADS, 4);
  // LL << "ZMQ_MAX_SOCKETS: " << zmq_ctx_get(context_, ZMQ_MAX_SOCKETS);

  CHECK(context_ != NULL) << "create 0mq context failed";
  bind();
  connect(my_node_);

#ifdef _DEBUG_VAN_
  debug_out_.open("van_"+my_node_.id());
#endif
}

void Van::destroy() {
  for (auto& it : senders_)
    zmq_close (it.second);
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

#ifdef _DEBUG_VAN_
  // if (van_filter.empty || van_filter==my_node_.id())
  // LL << my_node_.id() << ": binds address " << addr;
#endif
}

Status Van::connect(Node const& node) {
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

#ifdef _DEBUG_VAN_
  // LL << my_node_.id() << ": connect to " << addr;
#endif
  return Status::OK();
}

// TODO use zmq_msg_t to allow zero_copy send
// TODO socket is not thread safe!
Status Van::send(const Message& msg) {

  // find the socket
  NodeID id = msg.recver;
  auto it = senders_.find(id);
  if (it == senders_.end())
    return Status::NotFound("there is no socket to node " + (id));
  void *socket = it->second;

  // fill data
  auto task = msg.task;
  task.clear_uncompressed_size();
  bool has_key = !msg.key.empty();
  std::vector<SArray<char> > data;
  if (FLAGS_compress_message) {
    if (has_key) {
      data.push_back(msg.key.compressTo());
      task.add_uncompressed_size(msg.key.size());
    }
    for (auto& m : msg.value) {
      if (m.empty()) continue;
      data.push_back(m.compressTo());
      task.add_uncompressed_size(m.size());
    }
    for (int i = 0; i < data.size(); ++i) {
      send_uncompressed_ += task.uncompressed_size(i);
      send_compressed_ += data[i].size();
      // LL << task.uncompressed_size(i) << " " << data[i].size();
    }
  } else {
    if (has_key) data.push_back(msg.key);
    for (auto& m : msg.value) {
      if (m.empty()) continue;
      data.push_back(m);
    }
    for (int i = 0; i < data.size(); ++i) {
      send_uncompressed_ += data[i].size();
    }
  }

  // send task
  string str;
  task.set_has_key(has_key);
  CHECK(task.SerializeToString(&str))
      << "failed to serialize " << task.ShortDebugString();
  int tag = ZMQ_SNDMORE;
  if (data.size() == 0) tag = 0; // ZMQ_DONTWAIT;
  if (zmq_send(socket, str.c_str(), str.size(), tag) != str.size())
    return Status::NetError(
        "failed to send mailer to node " + (id) + zmq_strerror(errno));
  send_head_ += str.size();

  // send key and value
  for (int i = 0; i < data.size(); ++i) {
    const auto& raw = data[i];
    if (i == data.size() - 1) tag = 0; // ZMQ_DONTWAIT;
    if (zmq_send(socket, raw.data(), raw.size(), tag) != raw.size())
      return Status::NetError(
          "failed to send mailer to node " + (id) +
          zmq_strerror(errno));
  }

  // my_node_.id() == "S29" &&
  // if (msg.recver == "U0")
  //   LL << my_node_.id() << ">>>: " << msg.shortDebugString()<<"\n";
#ifdef _DEBUG_VAN_
  debug_out_ << my_node_.id() << ">>>: " << msg.shortDebugString()<< std::endl;
#endif
  // if (msg.task.time() == 22)
  // LL << my_node_.id() << ">>>: " << msg.shortDebugString()<<"\n";
  return Status::OK();
}

// TODO Zero copy
Status Van::recv(Message *msg) {
  msg->key = SArray<char>();
  msg->value.clear();
  NodeID sender;
  for (int i = 0; ; ++i) {
    zmq_msg_t zmsg;
    CHECK(zmq_msg_init(&zmsg) == 0) << zmq_strerror(errno);
    if (zmq_msg_recv(&zmsg, receiver_, 0) == -1)
      return Status::NetError(
          "recv message failed: " + std::string(zmq_strerror(errno)));
    char* buf = (char *)zmq_msg_data(&zmsg);
    CHECK(buf != NULL);
    size_t size = zmq_msg_size(&zmsg);

    if (i == 0) {
      // identify
      sender = id(std::string(buf, size));
      msg->sender = sender;
      msg->recver = my_node_.id();
    } else if (i == 1) {
      // task
      CHECK(msg->task.ParseFromString(std::string(buf, size)))
          << "parse string failed";
      recv_head_ += size;
    } else {
      // key and value
      SArray<char> data;
      int n = msg->task.uncompressed_size_size();
      if (n > 0) {
        // data are compressed
        CHECK_GT(n, i - 2);
        data.resize(msg->task.uncompressed_size(i-2)+16);
        data.uncompressFrom(buf, size);

        recv_compressed_ += size;
        recv_uncompressed_ += data.size();
      } else {
        // data are not compressed
        // data = SArray<char>(buf, buf+size);
        data.copyFrom(buf, size);

        recv_uncompressed_ += size;
      }

      if (i == 2 && msg->task.has_key()) {
        msg->key = data;
      } else {
        msg->value.push_back(data);
      }
    }
    zmq_msg_close(&zmsg);
    if (!zmq_msg_more(&zmsg)) { CHECK_GT(i, 0); break; }
  }

  // if (msg->task.type() == Task::CALL_CUSTOMER && msg->task.has_vector() &&
  //     msg->task.vector().cmd() == CallVec::DUPLICATE)
    // LL << *msg;
#ifdef _DEBUG_VAN_
  debug_out_ << my_node_.id() << "<<<: " << msg->shortDebugString() << std::endl;
#endif

  // if (msg->task.time() == 1442)
  // LL << my_node_.id() << "<<<: " << msg->shortDebugString();
  return Status::OK();;
}

void Van::statistic() {
  if (my_node_.role() == Node::UNUSED || my_node_.role() == Node::SCHEDULER) return;
  auto mb = [](size_t x) { return  (x * 10 / 1000000) / 10.0; };
  auto s = FLAGS_compress_message ? send_compressed_ : send_uncompressed_;
  auto r = FLAGS_compress_message ? recv_compressed_ : recv_uncompressed_;

  printf("%s sent %.1f Mb, received %.1f Mb\n",
         my_node_.id().c_str(), mb(s), mb(r));
  // printf("%s sent %.1f Mb, saved %.1f%%; received %.1f Mb, saved %.1f%%\n",
  //        my_node_.id().c_str(), mb(s), 100 - 100.0 * mb(s) / mb(send_uncompressed_),
  //        mb(r), 100 - 100.0 * mb(r) / mb(recv_uncompressed_));
}

} // namespace PS
