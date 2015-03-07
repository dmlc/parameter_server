#include "system/remote_node.h"
#include "system/customer.h"
#include "util/crc32c.h"
#include "util/shared_array_inl.h"
namespace PS {

DECLARE_bool(verbose);
DEFINE_bool(key_cache, true, "enable caching keys during communication");
DEFINE_bool(message_compression, true, "");

int RNode::size() {
  return (role() == Node::GROUP ? exec_.group(id()).size() : 1);
}

int RNode::submit(const MessagePtr& msg) {
  CHECK_NOTNULL(this); CHECK(msg);
  auto msgs = exec_.obj().slice(msg, exec_.keyRanges(id()));
  return submit(msgs);
}

int RNode::submit(const Task& task,
                  const Message::Callback& recv_handle) {
  MessagePtr msg(new Message(task));
  if (recv_handle) msg->recv_handle = recv_handle;
  return submit(msg);
}

int RNode::submitAndWait(const Task& task,
                         const Message::Callback& recv_handle) {
  MessagePtr msg(new Message(task));
  if (recv_handle) msg->recv_handle = recv_handle;
  msg->wait = true;
  return submit(msg);
}

int RNode::submit(const std::vector<Task>& tasks,
                  const Message::Callback& recv_handle) {
  MessagePtrList msgs; msgs.reserve(tasks.size());
  for (const auto& task : tasks) {
    msgs.push_back(MessagePtr(new Message(task)));
    if (recv_handle) msgs.back()->recv_handle = recv_handle;
  }
  return submit(msgs);
}

int RNode::submitAndWait(const std::vector<Task>& tasks,
                         const Message::Callback& recv_handle) {
  MessagePtrList msgs; msgs.reserve(tasks.size());
  for (const auto& task : tasks) {
    msgs.push_back(MessagePtr(new Message(task)));
    msgs.back()->wait = true;
    if (recv_handle) msgs.back()->recv_handle = recv_handle;
  }
  return submit(msgs);
}

int RNode::submit(MessagePtrList& msgs) {
  CHECK_NOTNULL(this);

  // choose a proper timestamp
  int t = Message::kInvalidTime;
  for (auto& msg : msgs) {
    CHECK(msg);
    auto& task = msg->task;
    // CHECK(task.has_type());
    // if there is a valid time in task, then all messages should have the same
    // time (to make my life easy)
    if (task.has_time() && task.time() > Message::kInvalidTime) {
      if (t != Message::kInvalidTime) {
        CHECK_EQ(t, task.time());
      } else {
        t = task.time();
      }
    }
  }
  {
    Lock l(mu_);
    if (t > Message::kInvalidTime) {
      time_ = std::max(t, time_);
    } else {
      if (role() == Node::GROUP) {
        for (auto w : exec_.group(id())) {
          // Lock l(w->time_mu_);
          time_ = std::max(w->time_, time_);
        }
      }
      t = ++time_;
    }
    if (msgs[0]->fin_handle) {
      msg_finish_handle_[t] = msgs[0]->fin_handle;
    }
  }

  // set some required fields
  for (auto& msg : msgs) {
    auto& task = msg->task;
    task.set_request(true);
    task.set_customer_id(exec_.obj().id());
    task.set_time(t);
    msg->original_recver = id();
    msg->sender = exec_.myNode().id();
  }

  // send messages one-by-one
  outgoing_task_.start(t);
  auto& rnodes = exec_.group(id());
  CHECK_EQ(rnodes.size(), msgs.size());
  for (int i = 0; i < msgs.size(); ++i) {
    auto& w = rnodes[i];
    {
      Lock l(w->mu_);
      msgs[i]->recver = w->id();
      w->time_ = std::max(t, w->time_);
      // a terminate confirm message will not get replied, so do not add it into
      // pending_msgs_
      // if (task.type() != Task::TERMINATE_CONFIRM)
      w->pending_msgs_[t] = msgs[i];
      if (msgs[i]->recv_handle) w->msg_receive_handle_[t] = msgs[i]->recv_handle;
    }
    w->encodeFilter(msgs[i]);
    sys_.queue(msgs[i]);
  }

  // wait if necessary
  for (int i = 0; i < msgs.size(); ++i) {
    if (msgs[i]->wait) rnodes[i]->outgoing_task_.wait(t);
  }
  return t;
}

void RNode::waitOutgoingTask(int time) {
  // if (time <= Message::kInvalidTime) return;
  for (auto& w : exec_.group(id())) {
    // LL << w->id();
    w->outgoing_task_.wait(time);
  }
}

bool RNode::tryWaitOutgoingTask(int time) {
  // if (time <= Message::kInvalidTime) return true;
  for (auto& w : exec_.group(id()))
    if (!w->outgoing_task_.tryWait(time)) return false;
  return true;
}

void RNode::finishOutgoingTask(int time) {
  for (auto& w : exec_.group(id())) w->outgoing_task_.finish(time);
}

void RNode::waitIncomingTask(int time) {
  // if (time <= Message::kInvalidTime) return;
  for (auto& w : exec_.group(id())) w->incoming_task_.wait(time);
}
bool RNode::tryWaitIncomingTask(int time) {
  // if (time <= Message::kInvalidTime) return true;
  for (auto& w : exec_.group(id()))
    if (!w->incoming_task_.tryWait(time)) return false;
  return true;
}

void RNode::finishIncomingTask(int time) {
  for (auto& w : exec_.group(id())) w->incoming_task_.finish(time);
  exec_.notify();
}

FilterPtr RNode::findFilter(const FilterConfig& conf) {
  Lock l(filter_mu_);
  int id = conf.type();
  auto it = filter_.find(id);
  if (it == filter_.end()) {
    filter_[id] = Filter::create(conf);
    it = filter_.find(id);
  }
  return it->second;
}

void RNode::encodeFilter(const MessagePtr& msg) {
  const auto& tk = msg->task;
  if (FLAGS_message_compression && !Filter::find(FilterConfig::COMPRESSING, msg)) {
    msg->addFilter(FilterConfig::COMPRESSING);
  }
  for (int i = 0; i < tk.filter_size(); ++i) {
    findFilter(tk.filter(i))->encode(msg);
  }
}
void RNode::decodeFilter(const MessagePtr& msg) {
  const auto& tk = msg->task;
  for (int i = tk.filter_size()-1; i >= 0; --i) {
    findFilter(tk.filter(i))->decode(msg);
  }
}

// void RNode::cacheKeySender(const MessagePtr& msg) {
//   if (!FLAGS_key_cache || !msg->task.has_key_range()) return;
//   auto cache_k = std::make_pair(
//       msg->task.key_channel(), Range<Key>(msg->task.key_range()));

//   if (msg->key.empty()) {
//     msg->task.clear_key_signature();
//     Lock l(key_cache_mu_);
//     key_cache_.erase(cache_k);
//     return;
//   }

//   auto sig = crc32c::Value(msg->key.data(), std::min(msg->key.size(), max_sig_len_));
//   msg->task.set_key_signature(sig);
//   Lock l(key_cache_mu_);
//   auto& cache = key_cache_[cache_k];
//   bool hit_cache = cache.first == sig && cache.second.size() == msg->key.size();
//   if (hit_cache) {
//     msg->key.clear();
//     msg->task.set_has_key(false);

//     if (FLAGS_verbose) {
//       LI << "cacheKeySender clears msg's key; msg [" <<
//         msg->shortDebugString() << "]";
//     }
//   } else {
//     cache.first = sig;
//     cache.second = msg->key;
//     msg->task.set_has_key(true);

//     if (FLAGS_verbose) {
//       LI << "cacheKeySender stores key for msg [" <<
//         msg->shortDebugString() << "]";
//     }
//   }

//   if (msg->task.erase_key_cache() && !msg->task.request()) key_cache_.erase(cache_k);
// }

// void RNode::cacheKeyRecver(const MessagePtr& msg) {
//   if (!FLAGS_key_cache || !msg->task.has_key_range()) return;
//   auto cache_k = std::make_pair(
//       msg->task.key_channel(), Range<Key>(msg->task.key_range()));
//   if (!msg->task.has_key_signature()) {
//     Lock l(key_cache_mu_);
//     key_cache_.erase(cache_k);

//     if (FLAGS_verbose) {
//       LI << "cacheKeyRecver erases key_cache_ item [" <<
//         cache_k.first << "{" << cache_k.second.begin() << "," <<
//         cache_k.second.end() << "}] " <<
//         "msg [" << msg->shortDebugString() << "]";
//     }
//     return;
//   }
//   auto sig = msg->task.key_signature();

//   Lock l(key_cache_mu_);
//   auto& cache = key_cache_[cache_k];
//   if (msg->task.has_key()) {
//     // double check
//     CHECK_EQ(crc32c::Value(msg->key.data(), std::min(msg->key.size(), max_sig_len_)), sig);
//     cache.first = sig;
//     cache.second = msg->key;

//     if (FLAGS_verbose) {
//       LI << "cacheKeyRecver stores key for msg [" << msg->shortDebugString() << "]";
//     }
//   } else {
//     // the cache is invalid... may ask the sender to resend this task
//     CHECK_EQ(sig, cache.first) << msg->debugString();
//     msg->key = cache.second;

//     if (FLAGS_verbose) {
//       LI << "cacheKeyRecver restores key for msg [" << msg->shortDebugString() << "]";
//     }
//   }
//   if (msg->task.erase_key_cache()) key_cache_.erase(cache_k);
// }

// size_t RNode::memSize() {
//   Lock l(key_cache_mu_);

//   size_t mem_size = 0;
//   for (const auto& item : key_cache_) {
//     mem_size += item.second.second.memSize();
//   }

//   return mem_size;
// }

} // namespace PS
