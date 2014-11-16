#include "system/executor.h"
#include "system/customer.h"
#include <thread>
namespace PS {

DECLARE_bool(verbose);

void Executor::init(const std::vector<Node>& nodes) {
  // insert virtual group nodes
  for (auto id : {
      kServerGroup, kWorkerGroup, kActiveGroup, kReplicaGroup, kOwnerGroup, kLiveGroup}) {
    Node node;
    node.set_role(Node::GROUP);
    node.set_id(id);
    add(node);
  }
  // insert real node
  for (auto& node : nodes) add(node);
  // sort the node in each group by its key range
  for (auto& it : node_groups_) {
    if (nodes_[it.first]->role() != Node::GROUP) continue;
    std::sort(it.second.begin(), it.second.end(), [](const RNodePtr& a, const RNodePtr& b) {
        return a->keyRange().begin() < b->keyRange().begin();
      });
  }
  // construct replica group and owner group (just after my_node_)
  if (my_node_.role() == Node::SERVER) {
    int my_pos = 0;
    auto servers = group(kServerGroup);
    int n = servers.size();
    for (auto s : servers) {
      if (s->node_.id() == my_node_.id()) break;
      ++ my_pos;
    }
    CHECK_LT(my_pos, n);

    int nrep = FLAGS_num_replicas; CHECK_LT(nrep, n);
    for (int i = 1; i <= nrep; ++i) {
      // the replica group is just before me
      node_groups_[kReplicaGroup].push_back(
          servers[my_pos - i < 0 ? n + my_pos - i : my_pos - i]);
      // the owner group is just after me
      node_groups_[kOwnerGroup].push_back(
          servers[my_pos + i < n ? my_pos + i : my_pos + i - n]);
    }
    // make an empty group otherwise
    if (nrep <= 0) {
      node_groups_[kReplicaGroup].clear();
      node_groups_[kOwnerGroup].clear();
    }
  }
  // store the key ranges in each group
  for (auto& it : node_groups_) {
    auto& partition = node_key_partition_[it.first];
    partition.reserve(it.second.size() + 1);
    for (auto w : it.second) {
      partition.push_back(w->keyRange().begin());
    }
    partition.push_back(
        it.second.size() > 0 ? it.second.back()->keyRange().end() : 0);
  }
}

void Executor::add(const Node& node) {
  auto id = node.id();
  CHECK_EQ(nodes_.count(id), 0);
  RNodePtr w(new RNode(node, *this));
  nodes_[id] = w;

  auto role = node.role();
  if (role != Node::GROUP) {
    node_groups_[id] = RNodePtrList({w});
    node_groups_[kLiveGroup].push_back(w);
  }
  if (role == Node::WORKER) {
    node_groups_[kWorkerGroup].push_back(w);
    node_groups_[kActiveGroup].push_back(w);
  } else if (role == Node::SERVER) {
    node_groups_[kServerGroup].push_back(w);
    node_groups_[kActiveGroup].push_back(w);
  }
}

string Executor::lastRecvReply() {
  CHECK(!active_msg_->task.request());
  CHECK_EQ(active_msg_->task.type(), Task::REPLY);
  return active_msg_->task.msg();
}

// a simple implementation of the DAG execution engine.
void Executor::run() {
  while (!done_) {
    bool process = false;

    if (FLAGS_verbose) {
      LI << obj_.name() << " before entering task loop; recved_msgs_. size [" <<
        recved_msgs_.size() << "]";
    }

    {
      std::unique_lock<std::mutex> lk(recved_msg_mu_);
      // pickup a message with dependency satisfied
      for (auto it = recved_msgs_.begin(); it != recved_msgs_.end(); ++it) {
        auto& msg = *it;
        auto sender = rnode(msg->sender);
        if (!sender) {
          LL << my_node_.id() << ": " << msg->sender
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

          if (FLAGS_verbose) {
            LI << obj_.name() << " picked up an active_msg_ from recved_msgs_. " <<
              "remaining size [" << recved_msgs_.size() << "]; msg [" <<
              active_msg_->shortDebugString() << "]";
          }
          break;
        }
      }
      if (!process) {
        if (FLAGS_verbose) {
          LI << obj_.name() << " picked nothing from recved_msgs_. size [" <<
            recved_msgs_.size() << "] waiting Executor::accept";
        }

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
    if (active_msg_->task.type() == Task::REPLY) {
        CHECK(!req) << "reply message should with request == false";
    } else {
      obj_.process(active_msg_);
    }
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
        if (FLAGS_verbose) {
          LI << "Task [" << t << "] completed. msg [" <<
            active_msg_->shortDebugString() << "]";
        }

        Message::Callback h;
        {
          Lock lk(o_recver->mu_);
          auto a = o_recver->msg_finish_handle_.find(t);
          if (a != o_recver->msg_finish_handle_.end()) h = a->second;
        }
        if (h) h();
      } else if (FLAGS_verbose) {
        LI << "Task [" << t << "] still running. msg [" <<
          active_msg_->shortDebugString() << "]";
      }
    }
  }
}


void Executor::accept(const MessagePtr& msg) {
  Lock l(recved_msg_mu_);
  auto sender = rnode(msg->sender); CHECK(sender) << msg->shortDebugString();
  sender->decodeFilter(msg);
  recved_msgs_.push_back(msg);
  dag_cond_.notify_one();
}

void Executor::replace(const Node& dead, const Node& live) {
  // TODO
  // auto dead_id = dead.id();
  // auto live_id = live.id();
  // if (live_id == my_node_.id()) return;

  // // FIXME update kLiveGroup
  // // modify
  // auto ptr = nodes_[dead_id];
  // CHECK(ptr.get() != nullptr);
  // Lock l(ptr->mu_);
  // ptr->node_ = live;

  // // insert the live
  // {
  //   Lock l2(node_mu_);
  //   nodes_[live_id] = ptr;
  //   node_groups_[live_id] = node_groups_[dead_id];
  //   node_key_partition_[live_id] = node_key_partition_[dead_id];

  //   // remove the dead
  //   CHECK(dead.role() != Node::GROUP);
  //   nodes_.erase(dead_id);
  //   node_groups_.erase(dead_id);
  //   node_key_partition_.erase(dead_id);

  //   // LL << my_node_.id() << ": " << obj_.name() << "'s node info is updated";
  // }

  // // resent unfinished tasks
  // ptr->clearCache();
  // if (ptr->pending_msgs_.size() > 0) {
  //   bool first = true;
  //   for (auto& it : ptr->pending_msgs_) {
  //     auto& msg = it.second;
  //     msg.recver = live_id;
  //     if (first) msg.task.set_wait_time(RNode::kInvalidTime);
  //     first = false;
  //     ptr->sys_.queue(ptr->cacheKeySender(msg));
  //     // LL << my_node_.id() << ": resent " << msg;
  //   }
  //   LL << my_node_.id() << ": re-sent " << ptr->pending_msgs_.size()
  //      << " pnending messages to " << live_id;
  // }
}


} // namespace PS
