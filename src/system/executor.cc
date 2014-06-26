#include "system/executor.h"
#include "system/customer.h"
#include <thread>
namespace PS {

void Executor::replace(const Node& dead, const Node& live) {
  auto dead_id = dead.id();
  auto live_id = live.id();
  if (live_id == my_node_.id()) return;

  // FIXME update kLiveGroup
  // modify
  auto ptr = nodes_[dead_id];
  CHECK(ptr.get() != nullptr);
  Lock l(ptr->mu_);
  ptr->node_ = live;

  // insert the live
  {
    Lock l2(node_mu_);
    nodes_[live_id] = ptr;
    node_groups_[live_id] = node_groups_[dead_id];
    node_key_partition_[live_id] = node_key_partition_[dead_id];

    // remove the dead
    CHECK(dead.role() != Node::GROUP);
    nodes_.erase(dead_id);
    node_groups_.erase(dead_id);
    node_key_partition_.erase(dead_id);

    // LL << my_node_.id() << ": " << obj_.name() << "'s node info is updated";
  }

  // resent unfinished tasks
  ptr->clearCache();
  if (ptr->pending_msgs_.size() > 0) {
    bool first = true;
    for (auto& it : ptr->pending_msgs_) {
      auto& msg = it.second;
      msg.recver = live_id;
      if (first) msg.task.set_wait_time(RNode::kInvalidTime);
      first = false;
      ptr->sys_.queue(ptr->cacheKeySender(msg));
      // LL << my_node_.id() << ": resent " << msg;
    }
    LL << my_node_.id() << ": re-sent " << ptr->pending_msgs_.size()
       << " pnending messages to " << live_id;
  }
}

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
    // LL << it.second.size();
    // for (auto x : it.second) LL << x->node_.DebugString();

    std::sort(it.second.begin(), it.second.end(), [](const RNodePtr& a, const RNodePtr& b) {
        // LL << a->node_.DebugString();
        // LL << b->node_.DebugString();
        return a->keyRange().begin() < b->keyRange().begin();
      });
  }

  // construct replica group and owner group (just after me)
  if (my_node_.role() == Node::SERVER) {
    int my_pos = 0;
    auto servers = group(kServerGroup);
    int n = servers.size();
    for (auto s : servers) {
      if (s->node_.id() == my_node_.id()) break;
      ++ my_pos;
    }
    CHECK_LT(my_pos, n);

    int nrep = FLAGS_num_replicas;
    CHECK_LT(nrep, n);

    for (int i = 1; i <= nrep; ++i) {
      // the replica group is just before me
      node_groups_[kReplicaGroup].push_back(
          servers[my_pos - i < 0 ? n + my_pos - i : my_pos - i]);
      // the owner group is just after me
      node_groups_[kOwnerGroup].push_back(
          servers[my_pos + i < n ? my_pos + i : my_pos + i - n]);
    }

    // if (FLAGS_num_servers > 1)
    //   LL << my_node_.id() << ": rep " << node_groups_[kReplicaGroup][0]->id()
    //      << ", own " << node_groups_[kOwnerGroup][0]->id();

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

  // LL << my_node_.id() << ": " << obj_.name() << " is ready";
}

void Executor::add(const Node& node) {
  auto id = node.id();
  CHECK_EQ(nodes_.count(id), 0);
  if (id == Postoffice::instance().myNode().id())
    my_node_ = node;

  RNodePtr w(new RNode(node, *this));
  nodes_[id] = w;

  auto role = node.role();

  if (role != Node::GROUP) {
    node_groups_[id] = RNodePtrList({w});
    node_groups_[kLiveGroup].push_back(w);
  }

  if (role == Node::CLIENT) {
    node_groups_[kWorkerGroup].push_back(w);
    node_groups_[kActiveGroup].push_back(w);
  } else if (role == Node::SERVER) {
    node_groups_[kServerGroup].push_back(w);
    node_groups_[kActiveGroup].push_back(w);
  }
}

// void Executor::remove(const Node& node) {
//   auto id = node.id();
//   CHECK_NE(nodes_.count(id), 0);
//   CHECK_NE(id, my_node_.id());

//   auto role = node.role();
//   if (role != Node::GROUP)
//     node_groups_.erase[id] = WorkerGroup({w});
// }

string Executor::lastRecvReply() {
  CHECK(!active_msg_.task.request());
  CHECK_EQ(active_msg_.task.type(), Task::REPLY);
  return active_msg_.task.msg();
}

// a naive implementation of DAG execution engine. It may has performance
// problem, do profile later TODO.
void Executor::run() {
  while (!done_) {
    bool do_process = false;
    {
      std::unique_lock<std::mutex> lk(recved_msg_mu_);
      // pickup a message with dependency satisfied
      for (auto it = recved_msgs_.begin(); it != recved_msgs_.end(); ++it) {
        int wait_time = it->task.wait_time();
        auto w = rnode(it->sender);
        if (!w) {
          LL << my_node_.id() << ": " << it->sender
             << " does not exist, ignore\n" << *it;
          recved_msgs_.erase(it);
          break;
        }
        if (!it->task.request() ||  // TODO rethink about it
            wait_time <= RNode::kInvalidTime ||
            w->tryWaitIncomingTask(wait_time)) {
          // if (it->task.type() != Task::REPLY &&
          //     worker(it->sender)->in_task_.hasFinished(it->task.time()))
          //   LL << it->debugString();
          do_process = true;
          active_msg_ = *it;
          recved_msgs_.erase(it);
          break;
        }
      }
      if (!do_process) {
        dag_cond_.wait(lk);
        // if (dag_cond_.wait_for(lk, std::chrono::seconds(5)) == std::cv_status::timeout) {
        //   LL << node().id() << " " << obj_.name() << ": timeout, recved #msg " << recved_msgs_.size();
        // }
        continue;
      }
    }
    // if (!do_process) continue;

      // process the picked message
      bool req = active_msg_.task.request();
      auto w = rnode(active_msg_.sender);
      int t = active_msg_.task.time();

      if (req) { w->incoming_task_.start(t); }

      if (active_msg_.task.type() == Task::REPLY)
        CHECK(!req) << "reply message should with request == false";
      else
        // call user program to process this message
        obj_.process(&active_msg_);

      if (req) {
        // if has been marked as finished, then mark it on in_task_; otherwise, the
        // user program need to set in_task_. see example in
        // LinearBlockIterator::updateModel when calling w_->roundTripForWorker
        if (active_msg_.finished) {
          w->incoming_task_.finish(t);
          notify();
          // reply an empty ack message if it has not been replied yet
          if (!active_msg_.replied) w->sys_.reply(active_msg_);
        }
      } else {
        RNode::Callback h;
        {
          Lock lk(w->mu_);
          auto b = w->msg_receive_handle_.find(t);
          if (b != w->msg_receive_handle_.end()) h = b->second;
        }
        if (h) h();

        w->outgoing_task_.finish(t);

        // the original receiver, such as the server group (all_servers)
        NodeID o_recver;
        {
          Lock l(w->mu_);
          auto it = w->pending_msgs_.find(t);
          CHECK(it != w->pending_msgs_.end())
              << " " << w->id() <<  " didn't find pending msg " << t << my_node_.id();
          o_recver = it->second.original_recver;
          w->pending_msgs_.erase(it);
        }

        auto o_w = rnode(o_recver);
        // LL << obj_.sid() << " try wait t " << t << ": " << o_w->tryWaitOutTask(t);
        if (o_w->tryWaitOutgoingTask(t)) {
          RNode::Callback h;
          {
            Lock lk(o_w->mu_);
            auto a = o_w->msg_finish_handle_.find(t);
            if (a != o_w->msg_finish_handle_.end()) h = a->second;
          }
          if (h) h();
          // if (t % 10 == 2 && !h && node().id() != "H") LL << node().id() << " " << t;
        }
      }
    // }
  }
}

void Executor::accept(const Message& msg) {
  Lock l(recved_msg_mu_);
  auto w = rnode(msg.sender);
  recved_msgs_.push_back(w->cacheKeyRecver(msg));
  // notify();
  dag_cond_.notify_one();

  // TODO sort it by priority
  // if (msg.task.priority() > 0)
  // recved_msgs_.push_front(msg);
  // else
}

} // namespace PS
