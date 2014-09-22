#include "system/remote_node.h"
#include "system/customer.h"
#include "util/crc32c.h"
#include "base/shared_array_inl.h"
namespace PS {

DEFINE_bool(key_cache, true, "enable caching keys during communication");
int RNode::submit(const MessagePtr& msg) {
  CHECK(msg);
  CHECK(msg->task.has_type());
  auto& task = msg->task;
  task.set_request(true);
  task.set_customer(exec_.obj().name());

  // set the message timestamp, store the finished_callback
  msg->original_recver = id();
  {
    Lock l(mu_);
    if (task.has_time()) {
      time_ = std::max(task.time(), time_);
    } else {
      // choose a timestamp
      if (role() == Node::GROUP) {
        for (auto w : exec_.group(id())) {
          // Lock l(w->time_mu_);
          time_ = std::max(w->time_, time_);
        }
      }
      task.set_time(++time_);
    }
    if (msg->fin_handle) msg_finish_handle_[task.time()] = msg->fin_handle;
  }

  int t = task.time();
  outgoing_task_.start(t);

  // partition the message according the receiver node's key range
  const auto& key_partition = exec_.partition(id());
  auto msgs = exec_.obj().slice(msg, key_partition);
  CHECK_EQ(msgs.size(), key_partition.size()-1);

  // sent partitioned messages one-by-one
  int i = 0;
  for (auto w : exec_.group(id())) {
    msgs[i]->sender = exec_.myNode().id();
    {
      Lock l(w->mu_);
      msgs[i]->recver = w->id();
      w->time_ = std::max(t, w->time_);
      // a terminate confirm message will not get replied
      // if (task.type() != Task::TERMINATE_CONFIRM)
      w->pending_msgs_[t] = msgs[i];
      if (msg->recv_handle) w->msg_receive_handle_[t] = msg->recv_handle;
    }
    w->cacheKeySender(msgs[i]);
    sys_.queue(msgs[i]);
    ++ i;
  }
  CHECK_EQ(i, msgs.size());

  if (msg->wait) waitOutgoingTask(t);
  return t;
}

void RNode::waitOutgoingTask(int time) {
  for (auto& w : exec_.group(id())) w->outgoing_task_.wait(time);
}

bool RNode::tryWaitOutgoingTask(int time) {
  for (auto& w : exec_.group(id()))
    if (!w->outgoing_task_.tryWait(time)) return false;
  return true;
}

void RNode::finishOutgoingTask(int time) {
  for (auto& w : exec_.group(id())) w->outgoing_task_.finish(time);
}

void RNode::waitIncomingTask(int time) {
  for (auto& w : exec_.group(id())) w->incoming_task_.wait(time);
}
bool RNode::tryWaitIncomingTask(int time) {
  for (auto& w : exec_.group(id()))
    if (!w->incoming_task_.tryWait(time)) return false;
  return true;
}

void RNode::finishIncomingTask(int time) {
  for (auto& w : exec_.group(id())) w->incoming_task_.finish(time);
  exec_.notify();
}


void RNode::cacheKeySender(const MessagePtr& msg) {
  if (!FLAGS_key_cache || !msg->task.has_key_range()) return;
  auto cache_k = std::make_pair(
      msg->task.key_channel(), Range<Key>(msg->task.key_range()));

  if (msg->key.empty()) {
    msg->task.clear_key_signature();
    Lock l(key_cache_mu_);
    key_cache_.erase(cache_k);
    return;
  }

  auto sig = crc32c::Value(msg->key.data(), msg->key.size());
  msg->task.set_key_signature(sig);
  Lock l(key_cache_mu_);
  auto& cache = key_cache_[cache_k];
  bool hit_cache = cache.first == sig && cache.second.size() == msg->key.size();
  if (hit_cache) {
    msg->key.clear();
    msg->task.set_has_key(false);
  } else {
    cache.first = sig;
    cache.second = msg->key;
    msg->task.set_has_key(true);
  }
}

void RNode::cacheKeyRecver(const MessagePtr& msg) {
  if (!FLAGS_key_cache || !msg->task.has_key_range()) return;
  auto cache_k = std::make_pair(
      msg->task.key_channel(), Range<Key>(msg->task.key_range()));
  if (!msg->task.has_key_signature()) {
    Lock l(key_cache_mu_);
    key_cache_.erase(cache_k);
    return;
  }
  auto sig = msg->task.key_signature();

  Lock l(key_cache_mu_);
  auto& cache = key_cache_[cache_k];
  if (msg->task.has_key()) {
    // double check
    CHECK_EQ(crc32c::Value(msg->key.data(), msg->key.size()), sig);
    cache.first = sig;
    cache.second = msg->key;
  } else {
    // the cache is invalid... may ask the sender to resend this task
    CHECK_EQ(sig, cache.first) << msg->debugString();
    msg->key = cache.second;
  }
}

} // namespace PS
