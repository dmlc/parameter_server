#include "system/executor.h"
#include "system/customer.h"
#include <thread>
namespace PS {

Executor::Executor(Customer& obj) : obj_(obj) {
  my_node_ = Postoffice::instance().manager().van().myNode();
  // insert virtual group nodes
  for (auto id : groupIDs()) {
    Node node;
    node.set_role(Node::GROUP);
    node.set_id(id);
    addNode(node);
  }

  thread_ = new std::thread(&Executor::run, this);
}

Executor::~Executor() {
  stop();
}

void Executor::stop() {
  if (!done_) {
    done_ = true;
    notify();
    thread_->join();
    delete thread_;
    thread_ = nullptr;
  }
}

void Executor::finish(const MessagePtr& msg) {
  RNode* r = rnode(msg->sender);
  if (r) r->finishIncomingTask(msg->task.time());
}

RNode* Executor::rnode(const NodeID& k) {
  Lock l(node_mu_);
  auto it = nodes_.find(k);
  return it == nodes_.end() ? NULL : it->second.node;
}

std::vector<RNode*>& Executor::group(const NodeID& k) {
  Lock l(node_mu_);
  auto it = nodes_.find(k);
  CHECK(it != nodes_.end()) << "unkonw node group: " << k;
  return it->second.sub_nodes;
}

const std::vector<Range<Key>>& Executor::keyRanges(const NodeID& k) {
  Lock l(node_mu_);
  auto it = nodes_.find(k);
  CHECK(it != nodes_.end()) << "unkonw node: " << k;
  return it->second.key_ranges;
}

void Executor::Manage(const MessagePtr& msg) {
  Lock l(node_mu_);
  CHECK(msg->task.has_ctrl());
  auto ctrl = msg->task.ctrl();
  switch (ctrl.cmd()) {
    case Control::ADD_NODE:
    case Control::UPDATE_NODE:
      CHECK_EQ(ctrl.node_size(), 1);
      AddNode(ctrl.node(0));
      break;
    case Control::REMOVE_NODE:
      CHECK_EQ(ctrl.node_size(), 1);
      RemoveNode(ctrl.node(0));
      break;
    case Control::REPLACE_NODE:
      CHECK_EQ(ctrl.node_size(), 2);
      ReplaceNode(ctrl.node(0), ctrl.node(1));
      break;
    default:
      CHECK(false) << msg->task.ShortDebugString();
  }
}

void Executor::ReplaceNode(const Node& old_node, const Node& new_node) {
  // TODO
}

void Executor::RemoveNode(const Node& node) {
  auto id = node.id();
  if (nodes_.find(id) == nodes_.end()) return;
  RNode* w = nodes_[id].node;
  for (const NodeID& gid : groupIDs()) {
    nodes_[gid].removeSubNode(w);
  }
  nodes_.erase(id);
}

void Executor::AddNode(const Node& node) {
  // add *node*
  if (node.id() == my_node_.id()) {
    my_node_ = node;
  }
  auto id = node.id();
  RNode* w = NULL;
  if (nodes_.find(id) != nodes_.end()) {
    // update
    w = nodes_[id].node;
    w->node_ = node;
    nodes_[id].removeSubNode(w);
    for (const NodeID& gid : groupIDs()) {
      nodes_[gid].removeSubNode(w);
    }
  } else {
    // create
    w = new RNode(node, *this);
    nodes_[id].node = w;
  }

  // add *node* into group
  auto role = node.role();
  if (role != Node::GROUP) {
    nodes_[id].addSubNode(w);
    nodes_[kLiveGroup].addSubNode(w);
  }
  if (role == Node::SERVER) {
    nodes_[kServerGroup].addSubNode(w);
    nodes_[kCompGroup].addSubNode(w);
  }
  if (role == Node::WORKER) {
    nodes_[kWorkerGroup].addSubNode(w);
    nodes_[kCompGroup].addSubNode(w);
  }

  // update replica group and owner group if necessary
  if (node.role() != Node::SERVER || my_node_.role() != Node::SERVER) return;
  if (num_replicas_ <= 0) return;

  const auto& servers = nodes_[kServerGroup];
  for (int i = 0; i < servers.sub_nodes.size(); ++i) {
    RNode* s = servers.sub_nodes[i];
    if (s->node_.id() != my_node_.id()) continue;

    // the replica group is just before me
    auto& replicas = nodes_[kReplicaGroup];
    replicas.clear();
    for (int j = std::max(i-num_replicas_, 0); j < i; ++ j) {
      replicas.sub_nodes.push_back(servers.sub_nodes[j]);
      replicas.key_ranges.push_back(servers.key_ranges[j]);
    }

    // the owner group is just after me
    auto& owners = nodes_[kOwnerGroup];
    owners.clear();
    for (int j = std::max(i-num_replicas_, 0); j < i; ++ j) {
      owners.sub_nodes.push_back(servers.sub_nodes[j]);
      owners.key_ranges.push_back(servers.key_ranges[j]);
    }
    break;
  }
}

bool Executor::PickActiveMsg() {
  std::unique_lock<std::mutex> lk(msg_mu_);
  auto it = recv_msgs_.begin();
  while (it != recv_msgs_.end()) {
    bool process = true;
    auto& msg = *it;

    // first check if the remote node is still alive. no locking is needed
    auto it2 = nodes_.find(msg->sender);
    if (it2 == nodes_.end()) {
      VLOG(WARNING) << my_node_.id() << ": rnode " << msg->sender <<
          " does not exist, ignore received message: " << msg->debugString();
      it = recv_msgs_.erase(it);
      continue;
    }
    auto& rnode = it2->second;
    if (!rnode.alive) {
      VLOG(WARNING) << my_node_.id() << ": rnode " << msg->sender <<
          " is not alive, ignore received message: " << msg->debugString();
      it = recv_msgs_.erase(it);
      continue;
    }

    // only check for request and non-control message. otherwise there is no
    // dependency constraint
    if (msg->task.request() && !msg->task.control()) {
      // check if the dependency constraints are satisfied
      for (int i = 0; i < msg->task.wait_time_size(); ++i) {
        int wait_time = msg->task.wait_time(i);
        if (wait_time <= Message::kInvalidTime) continue;
        std::lock_guard<std::mutex> lk2(node_mu_);
        if (!rnode.recv_req_tracker.IsFinished(wait_time)) {
          process = false;
          ++ it;
          break;
        }
      }
    }
    if (process) {
      active_msg_ = *it;
      recv_msgs_.erase(it);
      rnode.DecodeMessage(active_msg_);
      VLOG(2) << obj_.id() << " picked a messge in [" <<
          recv_msgs_.size() << "]. sent from " << msg->sender <<
          ": " << active_msg_->shortDebugString();
      return true;
    }
  }

  // sleep until received a new message or another message been marked as
  // finished.
  VLOG(2) << obj_.id() << " picked nothing. msg buffer size "
          << recv_msgs_.size();
  dag_cond_.wait(lk);
  return false;
}

void Executor::ProcessActiveMsg() {
  if (active_msg_->task.control()) {
    Manage(active_msg_);
    continue;
  }

  // ask the customer to process the picked message
  obj_.process(active_msg_);

  // postprocessing
  const NodeID& sender_id = active_msg_->sender;
  int ts = active_msg_->task.time();
  auto it2 = nodes_.find(sender_id);
  CHECK(it2 != nodes_.end()) << sender_id;
  auto& rnode = it2->second;

  if (active_msg_->task.request()) {
    // if this message is marked as finished, then set the mark in tracker,
    // otherwise, the user application need to call `Customer::FinishRecvReq`
    // to set the mark
    if (active_msg_->finished) {
      rnode.recv_req_tracker.Finish(ts);
      // reply an empty ACK message if necessary
      if (!active_msg_->replied) obj_.Reply(active_msg_);
    }
  } else {
    // find the original remote node this message goes to, it might be a group
    // node
    auto& orig_rnode = rnode;
    // FIXME
    // auto it3 = rnode.orig_recvers.find(ts);
    // if (it3 != rnode.orig_recvers.end()) {
    //   auto it4 = nodes_.find(it3->second);
    //   CHECK(it4 != nodes_.end())
    //       << "node " << it3->second << " does not exists";
    //   orig_rnode = it4->second;
    // }

    // call the callback if valid
    auto it5 = orig_rnode.sent_req_callbacks.find(ts);
    if (it5 != orig_rnode.sent_req_callbacks.end()) {
      if (it5->second) it5->second();
      orig_rnode.sent_req_callbacks.erase(it5);
    }
  }
}

void Executor::Accept(const MessagePtr& msg) {
  Lock l(msg_mu_);
  recv_msgs_.push_back(msg);
  dag_cond_.notify_one();
}

void Executor::NodeInfo::addSubNode(RNode* s) {
  CHECK_NOTNULL(s);
  // insert s into sub_nodes such as sub_nodes is still ordered
  int pos = 0;
  while (pos < sub_nodes.size() &&
         CHECK_NOTNULL(sub_nodes[pos])->keyRange().inLeft(s->keyRange())) {
    ++ pos;
  }
  sub_nodes.insert(sub_nodes.begin()+pos, s);
  // update the key range
  key_ranges.insert(key_ranges.begin() + pos, s->keyRange());
}

void Executor::NodeInfo::removeSubNode(RNode* s) {
  size_t n = sub_nodes.size();
  CHECK_EQ(n, key_ranges.size());
  for (int i = 0; i < n; ++i) {
    if (sub_nodes[i] == s) {
      sub_nodes.erase(sub_nodes.begin() + i);
      key_ranges.erase(key_ranges.begin() + i);
      break;
    }
  }
}


} // namespace PS
