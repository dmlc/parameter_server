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

  thread_ = unique_ptr<std::thread>(new std::thread(&Executor::run, this));
}

Executor::~Executor() {
  stop();
}

void Executor::stop() {
  if (!done_) {
    done_ = true;
    notify();
    thread_->join();
    thread_.release();
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

void Executor::copyNodesFrom(const Executor& other) {
  for (const auto& n : other.nodes_) {
    auto d = n.second.node->node_;
    if (d.role() != Node::GROUP) addNode(d);
  }
}

void Executor::replaceNode(const Node& old_node, const Node& new_node) {
  // TODO
}

void Executor::removeNode(const Node& node) {
  Lock l(node_mu_);
  auto id = node.id();
  if (nodes_.find(id) == nodes_.end()) return;
  RNode* w = nodes_[id].node;
  for (const NodeID& gid : groupIDs()) {
    nodes_[gid].removeSubNode(w);
  }
  nodes_.erase(id);
}

void Executor::addNode(const Node& node) {
  Lock l(node_mu_);
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

  // update replica group and owner group if i'm a server node
  // TODO
  // if (my_node_.role() == Node::SERVER) {
  //   int my_pos = 0;
  //   auto servers = group(kServerGroup);
  //   int n = servers.size();
  //   for (auto s : servers) {
  //     if (s->node_.id() == my_node_.id()) break;
  //     ++ my_pos;
  //   }
  //   CHECK_LT(my_pos, n);

  //   int nrep = FLAGS_num_replicas; CHECK_LT(nrep, n);
  //   for (int i = 1; i <= nrep; ++i) {
  //     // the replica group is just before me
  //     node_groups_[kReplicaGroup].push_back(
  //         servers[my_pos - i < 0 ? n + my_pos - i : my_pos - i]);
  //     // the owner group is just after me
  //     node_groups_[kOwnerGroup].push_back(
  //         servers[my_pos + i < n ? my_pos + i : my_pos + i - n]);
  //   }
  //   // make an empty group otherwise
  //   if (nrep <= 0) {
  //     node_groups_[kReplicaGroup].clear();
  //     node_groups_[kOwnerGroup].clear();
  //   }
  // }
}

// string Executor::lastRecvReply() {
//   CHECK(!active_msg_->task.request());
//   return active_msg_->task.msg();
// }

// a simple implementation of the DAG execution engine.
void Executor::run() {
  while (!done_) {
    bool process = false;
    VLOG(2) << obj_.id() << " before entering task loop; recved_msgs_. size [" <<
        recved_msgs_.size() << "]";
    {
      std::unique_lock<std::mutex> lk(recved_msg_mu_);
      // pickup a message with dependency satisfied
      for (auto it = recved_msgs_.begin(); it != recved_msgs_.end(); ++it) {
        auto& msg = *it;
        auto sender = rnode(msg->sender);
        if (!sender) {
          LOG(WARNING) << my_node_.id() << ": " << msg->sender
                       << " does not exist, ignore\n" << msg->debugString();
          recved_msgs_.erase(it);
          break;
        }
        // ack message, no dependency constraint
        process = !msg->task.request();
        if (!process) {
          // check if the dependency constraints are satisfied
          bool satisfied = true;
          for (int i = 0; i < msg->task.wait_time_size(); ++i) {
            int wait_time = msg->task.wait_time(i);
            if (wait_time > Message::kInvalidTime &&
                !sender->tryWaitIncomingTask(wait_time)) {
              satisfied = false;
            }
          }
          process = satisfied;
        }
        if (process) {
          active_msg_ = msg;
          recved_msgs_.erase(it);
          VLOG(2) << obj_.id() << " picked up an active_msg_ from recved_msgs_. " <<
              "remaining size [" << recved_msgs_.size() << "]; msg [" <<
              active_msg_->shortDebugString() << "]";
          break;
        }
      }
      if (!process) {
        VLOG(2) << obj_.id() << " picked nothing from recved_msgs_. size [" <<
            recved_msgs_.size() << "] waiting Executor::accept";
        dag_cond_.wait(lk);
        continue;
      }
    }
    // process the picked message
    bool req = active_msg_->task.request();
    int t = active_msg_->task.time();
    auto sender = rnode(active_msg_->sender);
    CHECK(sender) << "unknow node: " << active_msg_->sender;
    // mark it as been started in the task tracker
    if (req) sender->incoming_task_.start(t);
    // call user program to process this message if necessary
    obj_.process(active_msg_);
    if (req) {
      // if this message is finished, then mark it in the task tracker,
      // otherwise, it is the user program's job to mark it.
      if (active_msg_->finished) {
        sender->incoming_task_.finish(t);
        notify();
        // reply an empty ack message if it has not been replied yet
        if (!active_msg_->replied)
          sender->sys_.reply(active_msg_->sender, active_msg_->task);
      }
    } else {
      // run the receiving callback if necessary
      Message::Callback h;
      {
        Lock lk(sender->mu_);
        auto b = sender->msg_receive_handle_.find(t);
        if (b != sender->msg_receive_handle_.end()) h = b->second;
      }
      if (h) h();
      // mark this out going task as finished
      sender->outgoing_task_.finish(t);
      // find the original receiver, such as the server group (all_servers)
      NodeID original_recver_id;
      {
        Lock l(sender->mu_);
        auto it = sender->pending_msgs_.find(t);
        CHECK(it != sender->pending_msgs_.end())
            << myNode().id() << ": there is no message has been sent to "
            << sender->id() << " on time " << t;
        original_recver_id = it->second->original_recver;
        sender->pending_msgs_.erase(it);
      }
      // run the finishing callback if necessary
      auto o_recver = rnode(original_recver_id);
      CHECK(o_recver) << "no such node: " << original_recver_id;
      if (o_recver->tryWaitOutgoingTask(t)) {
        VLOG(2) << "Task [" << t << "] completed. msg [" <<
            active_msg_->shortDebugString() << "]";
        Message::Callback h;
        {
          Lock lk(o_recver->mu_);
          auto a = o_recver->msg_finish_handle_.find(t);
          if (a != o_recver->msg_finish_handle_.end()) h = a->second;
        }
        if (h) h();
      } else {
        VLOG(2) << "Task [" << t << "] still running. msg [" <<
            active_msg_->shortDebugString() << "]";
      }
    }
  }
}


void Executor::accept(const MessagePtr& msg) {
  Lock l(recved_msg_mu_);
  auto sender = rnode(msg->sender);
  if (!sender) return;
  sender->decodeFilter(msg);
  recved_msgs_.push_back(msg);
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
