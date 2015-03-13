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
    AddNode(node);
  }

  thread_ = new std::thread(&Executor::Run, this);
}

Executor::~Executor() {
  if (done_) return;
  done_ = true;
  dag_cond_.notify_all();  // wake thread_
  thread_->join();
  delete thread_;
}

void Executor::WaitSentReq(int timestamp, const NodeID& recver) {
  std::unique_lock<std::mutex> lk(node_mu_);
  auto rnode = GetRNode(recver);
  rnode->cond.wait(lk, [rnode] {
      return !rnode->alive || rnode->sent_req_tracker.IsFinished(timestamp); });
}

void Executor::WaitRecvReq(int timestamp, const NodeID& sender) {
  std::unique_lock<std::mutex> lk(node_mu_);
  auto rnode = GetRNode(sender);
  rnode->cond.wait(lk, [rnode] {
      return !rnode->alive || rnode->recv_req_tracker.IsFinished(timestamp); });
}

void Executor::FinishRecvReq(int timestamp, const NodeID& sender) {
  Lock lk(node_mu_);
  auto rnode = GetRNode(sender);
  rnode->recv_req_tracker.Finish(timestamp);
  lk.unlock();
  rnode->cond.notify_all();
}


int Executor::Submit(const MessagePtr& msg) {
  Lock l(node_mu_);
  RNodeGroup* grp = FindGroup(msg->sender);
  CHECK(grp != nullptr) << msg->sender << " doesn't exist";

  // timestamp
  int ts = msg->task.has_time() ? msg->task.time() : Message::kInvalidTime;
  if (ts <= Message::kInvalidTime) {
    // pick a proper timestamp
    for (auto r : grp->nodes) ts = std::max(ts, r->time+1);
  }
  // update the timestamp
  for (auto r : grp->nodes) {
    CHECK_LT(r->time, ts) << my_node_.id() <<
        ": node " << r->rnode.id() << " has a newer timstamp";
    r->time = ts;
  }
  msg->task.set_time(ts);

  // set and store something
  msg->task.set_request(true);
  msg->task.set_customer_id(obj_.id());
  if (msg->fin_handle) {
    CHECK_NOTNULL(GetRNode(msg->sender))->SetCallback(ts, msg->fin_handle);
  }

  // slice "msg" and then send them one by one
  MessagePtrList msgs(grp->keys.size());
  obj_.Slice(msg, grp->keys, &msgs);
  CHECK_EQ(msgs.size(), grp->nodes.size());
  for (int i = 0; i < msgs.size(); ++i) {
    RemoteNode* rnode = CHECK_NOTNULL(grp->nodes[i]);
    MessagePtr& m = msgs[i];
    if (!m.valid) {
      // do not sent, just mark it as done
      rnode->sent_req_tracker.Finish(ts);
      continue;
    }
    const NodeID& recv = rnode->rnode.id();  // the actual receiver of "m"
    if (msg->recv != recv) {
      // that means "msg" is sent to a group node. save this info
      rnode->sent_to_group[ts] = msg->recv;
    }
    rnode->EncodeMessage(m);
    m->recv = recv;
    sys_.queue(m);
  }

  return ts;
}


bool Executor::PickActiveMsg() {
  std::unique_lock<std::mutex> lk(msg_mu_);
  auto it = recv_msgs_.begin();
  while (it != recv_msgs_.end()) {
    bool process = true;
    auto& msg = *it;

    // check if the remote node is still alive.
    Lock l(node_mu_);
    auto rnode = GetRNode(msg->sender);
    if (!rnode->alive) {
      VLOG(WARNING) << my_node_.id() << ": rnode " << msg->sender <<
          " is not alive, ignore received message: " << msg->debugString();
      it = recv_msgs_.erase(it);
      continue;
    }
    // check if double receiving
    bool req = msg->task.request();
    int ts = msg->task.time();
    if ((req && rnode->recv_req_tracker.IsFinished(ts)) ||
        (!req && rnode->sent_req_tracker.IsFinished(ts))) {
      VLOG(WARNING) << my_node_.id() << ": doubly received message. ignore: " <<
          msg->debugString();
      it = recv_msgs_.erase(it);
      continue;
    }

    // check for dependency constraint. only needed for request and non-control message.
    if (req && !msg->task.control()) {
      for (int i = 0; i < msg->task.wait_time_size(); ++i) {
        int wait_time = msg->task.wait_time(i);
        if (wait_time <= Message::kInvalidTime) continue;
        if (!rnode->recv_req_tracker.IsFinished(wait_time)) {
          process = false;
          ++ it;
          break;
        }
      }
    }
    if (process) {
      active_msg_ = *it;
      recv_msgs_.erase(it);
      rnode->DecodeMessage(active_msg_);
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
    return;
  }

  // ask the customer to process the picked message
  obj_.process(active_msg_);

  // postprocessing
  int ts = active_msg_->task.time();

  if (active_msg_->task.request()) {
    if (active_msg_->finished) {
      // if this message is marked as finished, then set the mark in tracker,
      // otherwise, the user application need to call `Customer::FinishRecvReq`
      // to set the mark
      FinishRecvReq(ts, active_msg_->sender);
      // reply an empty ACK message if necessary
      if (!active_msg_->replied) obj_.Reply(active_msg_);
    }
  } else {
    Lock l(node_mu_);
    // mark as finished
    auto rnode = GetRNode(active_msg_->sender);
    rnode->sent_req_tracker.Finish(ts);

    // run callback if necessary
    auto& groups = rnode->sent_to_group;
    auto it = groups.find(ts);
    if (it != groups.end()) {
      // this message was sent to a group node. so "ts" is finished only if all
      // nodes in this group is finished
      bool done = true;
      anto onode = GetNode(it->second);
      for (auto r : onode->nodes) {
        if (r->alive && !r->sent_req_tracker.IsFinished(ts)) {
          done = false;
          break;
        }
      }
      if (!done) return;
      onode->sent_req_tracker.Finish(ts);
      onode->RunCallback(ts);
      groups.erase(it);
      l.unlock();
      onode->cond.notify_all();
    } else {
      rnode->RunCallback(ts);
      l.unlock();
      rnode->cond.notify_all();
    }
  }
}

void Executor::Accept(const MessagePtr& msg) {
  Lock l(msg_mu_);
  recv_msgs_.push_back(msg);
  dag_cond_.notify_one();
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
  auto r = GetRNode(id);
  for (const NodeID& gid : groupIDs()) {
    nodes_[gid].RemoveSubNode(r);
  }
  // do not remove r from nodes_
  r->alive = false;
}

void Executor::AddNode(const Node& node) {
  // add "node"
  if (node.id() == my_node_.id()) {
    my_node_ = node;
  }
  auto id = node.id();
  if (nodes_.find(id) != nodes_.end()) {
    // update
    auto r = GetRNode(id);
    CHECK(r->alive);
    r->rnode = node;
    for (const NodeID& gid : groupIDs()) {
      nodes_[gid].RemoveSubNode(r);
    }
  } else {
    // create
    nodes_[id].rnode = node;
  }

  // add "node" into group
  auto role = node.role();
  auto w = GetRNode(id);
  if (role != Node::GROUP) {
    nodes_[id].AddSubNode(w); nodes_[kLiveGroup].AddSubNode(w);
  }
  if (role == Node::SERVER) {
    nodes_[kServerGroup].AddSubNode(w); nodes_[kCompGroup].AddSubNode(w);
  }
  if (role == Node::WORKER) {
    nodes_[kWorkerGroup].AddSubNode(w); nodes_[kCompGroup].AddSubNode(w);
  }

  // update replica group and owner group if necessary
  if (node.role() != Node::SERVER || my_node_.role() != Node::SERVER) return;
  if (num_replicas_ <= 0) return;

  const auto& servers = nodes_[kServerGroup];
  for (int i = 0; i < servers.nodes.size(); ++i) {
    auto s = servers.nodes[i];
    if (s->rnode.id() != my_node_.id()) continue;

    // the replica group is just before me
    auto& replicas = nodes_[kReplicaGroup];
    replicas.nodes.clear(); replicas.keys.clear();
    for (int j = std::max(i-num_replicas_, 0); j < i; ++ j) {
      replicas.nodes.push_back(servers.nodes[j]);
      replicas.keys.push_back(servers.keys[j]);
    }

    // the owner group is just after me
    auto& owners = nodes_[kOwnerGroup];
    owners.nodes.clear(); owners.keys.clear();
    for (int j = std::max(i-num_replicas_, 0); j < i; ++ j) {
      owners.nodes.push_back(servers.nodes[j]);
      owners.keys.push_back(servers.keys[j]);
    }
    break;
  }
}



// void Executor::finish(const MessagePtr& msg) {
//   RNode* r = rnode(msg->sender);
//   if (r) r->finishIncomingTask(msg->task.time());
// }

// RNode* Executor::rnode(const NodeID& k) {
//   Lock l(node_mu_);
//   auto it = nodes_.find(k);
//   return it == nodes_.end() ? NULL : it->second.node;
// }

// std::vector<RNode*>& Executor::group(const NodeID& k) {
//   Lock l(node_mu_);
//   auto it = nodes_.find(k);
//   CHECK(it != nodes_.end()) << "unkonw node group: " << k;
//   return it->second.sub_nodes;
// }


} // namespace PS
