#include "system/remote_node.h"
#include "system/customer.h"
#include "util/crc32c.h"
#include "base/shared_array_inl.h"
namespace PS {

DEFINE_bool(key_cache, true, "enable caching keys during communication");

int RNode::submit(Message msg, Callback before, Callback after, bool no_wait) {
  auto& tk = msg.task;
  CHECK(tk.has_type());
  tk.set_request(true);
  tk.set_customer(exec_.obj().name());

  msg.original_recver = id();
  {
    // set the message timestamp
    Lock l(mu_);
    if (tk.has_time()) {
      // if the timestamp has been set, just trust it.
      time_ = std::max(tk.time(), time_);
    } else {
      // choose a timestamp
      if (role() == Node::GROUP) {
        for (auto w : exec_.group(id())) {
          // Lock l(w->time_mu_);
          time_ = std::max(w->time_, time_);
        }
      }
      tk.set_time(++time_);
    }
    if (after) msg_finish_handle_[tk.time()] = after;
  }

  int t = tk.time();
  outgoing_task_.start(t);

  // partition the message according the receiver node's key range
  auto key_partition = exec_.partition(id());
  auto msgs = exec_.obj().decompose(msg, key_partition);
  CHECK_EQ(msgs.size(), key_partition.size()-1);

  // sent partitioned messages one-by-one
  int i = 0;
  for (auto w : exec_.group(id())) {
    msgs[i].sender = exec_.myNode().id();
    {
      Lock l(w->mu_);
      msgs[i].recver = w->id();
      w->time_ = std::max(t, w->time_);
      // do not pending it, it will not be replied
      if (tk.type() != Task::TERMINATE_CONFIRM)
        w->pending_msgs_[t] = msgs[i];
      if (before) w->msg_receive_handle_[t] = before;
    }
    sys_.queue(w->cacheKeySender(msgs[i]));
  // if (tk.shared_para().cmd() == CallSharedPara::PUSH_REPLICA ) {
  //   LL << msgs[i];
  // }
    ++ i;
  }
  CHECK_EQ(i, msgs.size());

  if (!no_wait) waitOutgoingTask(t);

  return t;
}

void RNode::waitOutgoingTask(int time) {
  for (auto& w : exec_.group(id()))
    w->outgoing_task_.wait(time);
}

bool RNode::tryWaitOutgoingTask(int time) {
  for (auto& w : exec_.group(id()))
    if (!w->outgoing_task_.tryWait(time))
      return false;
  return true;
}

void RNode::finishOutgoingTask(int time) {
  for (auto& w : exec_.group(id()))
    w->outgoing_task_.finish(time);
}

void RNode::waitIncomingTask(int time) {
  for (auto& w : exec_.group(id()))
    w->incoming_task_.wait(time);
}
bool RNode::tryWaitIncomingTask(int time) {
  for (auto& w : exec_.group(id()))
    if (!w->incoming_task_.tryWait(time))
      return false;
  return true;
}

void RNode::finishIncomingTask(int time) {
  for (auto& w : exec_.group(id()))
    w->incoming_task_.finish(time);
  exec_.notify();
}

// Message RNode::cache(const Message& msg) {
//  if (!FLAGS_key_cache || msg.key.size() == 0) return msg;

//   // CHECK(msg.task.has_
//   return msg;
// }

Message RNode::cacheKeySender(const Message& msg) {
  if (!FLAGS_key_cache || !msg.task.has_key_range() || msg.key.size() == 0)
    return msg;

  Message ret = msg;
  Range<Key> range(ret.task.key_range());
  auto sig = crc32c::Value(ret.key.data(), ret.key.size());

  // Lock l(key_cache_mu_);
  auto& cache = key_cache_[range];

  bool hit_cache = cache.first == sig && cache.second.size() == ret.key.size();
  if (hit_cache) {
    ret.key.reset(nullptr, 0);
    ret.task.set_has_key(false);
  } else {
    cache.first = sig;
    cache.second = ret.key;
    ret.task.set_has_key(true);
  }

  ret.task.set_key_signature(sig);
  return ret;
}

Message RNode::cacheKeyRecver(const Message& msg) {
  if (!FLAGS_key_cache || !msg.task.has_key_range()) return msg;

  Message ret = msg;
  Range<Key> range(ret.task.key_range());
  auto sig = ret.task.key_signature();

  // Lock l(key_cache_mu_);
  auto& cache = key_cache_[range];

  if (ret.task.has_key()) {
    CHECK_EQ(crc32c::Value(ret.key.data(), ret.key.size()), sig);
    cache.first = sig;
    cache.second = ret.key;
  } else {
    CHECK_EQ(sig, cache.first);
    ret.key = cache.second;
  }
  return ret;
}

} // namespace PS
