#include "system/remote_node.h"
#include "system/customer.h"

namespace PS {

DEFINE_bool(key_cache, true, "enable caching keys during communication");

int RNode::submit(Message msg, Callback before, Callback after, bool no_wait) {
  auto& tk = msg.task;
  CHECK(tk.has_type());
  tk.set_request(true);
  tk.set_customer(exec_.obj().name());

  {
    Lock l(mu_);
    msg.original_recver = id();
    if (tk.has_time()) {
      // if timestamp has been set, just trust it.
      time_ = std::max(tk.time(), time_);
    } else {
      // choose a timestamp
      if (role() == Node::GROUP) {
        for (auto w : exec_.group(id())) {
          Lock l(w->mu_);  // seems not necessary
          time_ = std::max(w->time_, time_);
        }
      }
      tk.set_time(++time_);
    }
    if (after) cb_after_[tk.time()] = after;
  }
  int t = tk.time();
  out_task_.start(t);

  // partition the message according the receiver node's key range
  auto key_partition = exec_.partition(id());
  auto msgs = exec_.obj().decompose(msg, key_partition);
  CHECK_EQ(msgs.size(), key_partition.size()-1);

  // sent partitioned messages one-by-one
  int i = 0;
  for (auto w : exec_.group(id())) {
    msgs[i].sender = exec_.node().id();
    {
      Lock l(w->mu_);
      msgs[i].recver = w->id();
      w->time_ = std::max(t, w->time_);
      // do not pending it, it will not get replied
      if (tk.type() != Task::TERMINATE_CONFIRM)
        w->pending_msgs_[t] = msgs[i];
      if (before) w->cb_before_[t] = before;
      sys_.queue(w->cache(msgs[i]));
    }
  // if (tk.shared_para().cmd() == CallSharedPara::PUSH_REPLICA ) {
  //   LL << msgs[i];
  // }
    ++ i;
  }
  CHECK_EQ(i, msgs.size());

  if (!no_wait) waitOutTask(t);

  return t;
}

void RNode::waitOutTask(int time) {
  for (auto& w : exec_.group(id()))
    w->out_task_.wait(time);
}

bool RNode::tryWaitOutTask(int time) {
  for (auto& w : exec_.group(id()))
    if (!w->out_task_.tryWait(time))
      return false;
  return true;
}

void RNode::finishOutTask(int time) {
  for (auto& w : exec_.group(id()))
    w->out_task_.finish(time);
}

void RNode::waitInTask(int time) {
  for (auto& w : exec_.group(id()))
    w->in_task_.wait(time);
}
bool RNode::tryWaitInTask(int time) {
  for (auto& w : exec_.group(id()))
    if (!w->in_task_.tryWait(time))
      return false;
  return true;
}

void RNode::finishInTask(int time) {
  for (auto& w : exec_.group(id()))
    w->in_task_.finish(time);
}

Message RNode::cache(const Message& msg) {
 // if (!FLAGS_key_cache || msg->key.size() == 0) return msg;

  // CHECK(msg.task.has_
  return msg;
}

void RNode::clearCache() {
}

} // namespace PS
