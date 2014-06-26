#pragma once

#include "system/customer.h"

namespace PS {

template <typename K, typename V>
class SharedParameter : public Customer {
 public:

  // process a received message, will called by executor's thread
  void process(Message* msg);

  //
  std::vector<Message> decompose(const Message& msg, const Keys& partition);

  typedef std::function<void()> Fn;
  // return the timestamp of the sending message
  int sync(
      // Push, pull, ...
      CallSharedPara_Command cmd,
      // densitination node
      const NodeID& dest,
      // global key range
      Range<K> range,
      // must has key_ and (may empty) value_, all other entries are optional
      Message msg,
      // optional argments:
      int time = -1,
      int wait_time = -1,
      // Will be called if anythings goes back from the dest node but before
      // this task has been marked as finished
      Fn recv_handle = Fn(),
      // Will be called this task has been finished. if the dest node is a
      // group, then it means I have received data from all nodes in this group
      Fn fin_handle = Fn(),
      // block or non-block
      bool no_wait = true) {
    setCall(&(msg.task))->set_cmd(cmd);
    range.to(msg.task.mutable_key_range());
    if (time >= 0) msg.task.set_time(time);
    msg.task.set_wait_time(wait_time);

    return taskpool(dest)->submit(msg, recv_handle, fin_handle, no_wait);
  }


 protected:

  virtual std::vector<Message> decomposeTemplate(
      const Message& msg, const std::vector<K>& partition) = 0;

  // fill the values specified by the key lists in msg
  virtual void getValue(Message* msg) = 0;

  // set the received KV pairs into my data strcuture
  virtual void setValue(Message* msg) = 0;

  // the message contains the backup KV pairs sent by the master node of the key
  // segment to its replica node. merge these pairs into my replica, say
  // replica_[msg->sender] = ...
  virtual void setReplica(Message *msg) = 0; // { CHECK(false); }

  // retrieve the replica. a new server node replacing a dead server will first
  // ask for the dead's replica node for the data
  virtual void getReplica(Range<K> range, Message *msg)  = 0; // {CHECK(false);  }

  // a new server node fill its own datastructure via the the replica data from
  // the dead's replica node
  virtual void recoverFrom(Message *msg) = 0; // { CHECK(false); }

  // recover from a replica node
  void recover(Range<K> range) {
    // TODO recover from checkpoint

    CHECK_GT(FLAGS_num_replicas, 0);
    Task task;
    auto arg = setCall(&task);
    arg->set_cmd(CallSharedPara::PULL_REPLICA);
    range.to(arg->mutable_key());

    auto slave = exec_.group(kReplicaGroup)[0];
    slave->submitAndWait(task);

    for (auto owner : exec_.group(kOwnerGroup)) {
      LL << "ask for " << owner->id() << " for replica";
      keyRange(owner->id()).to(arg->mutable_key());
      owner->submitAndWait(task);
      LL << "done";
    }
  }

  // query the key range of a node
  Range<K> myKeyRange() {
    return keyRange(Customer::myNodeID());
  }

  Range<K> keyRange(const NodeID& id) {
    return Range<K>(exec_.rnode(id)->keyRange());
  }

  CallSharedPara getCall(const Message& msg) {
    CHECK_EQ(msg.task.type(), Task::CALL_CUSTOMER);
    CHECK(msg.task.has_shared_para());
    return msg.task.shared_para();
  }

  CallSharedPara* setCall(Message *msg) {
    return setCall(&(msg->task));
  }
  CallSharedPara* setCall(Task *task) {
    task->set_type(Task::CALL_CUSTOMER);
    return task->mutable_shared_para();
  }

 private:

  // add key_range in the future, it is not necessary now
  std::unordered_map<NodeID, std::vector<int> > clock_replica_;
};

template <typename K, typename V>
std::vector<Message> SharedParameter<K,V>::decompose(
    const Message& msg, const Keys& partition) {
  auto cmd = getCall(msg).cmd();
  if (cmd == CallSharedPara::PUSH_REPLICA ||
      cmd == CallSharedPara::PULL_REPLICA) {
    return Customer::decompose(msg, partition);
  }

  CHECK(std::is_sorted(partition.begin(), partition.end()));
  auto kr = Range<K>(msg.task.key_range());
  std::vector<K> keys;
  keys.reserve(partition.size());
  for (auto k : partition)
    keys.push_back(std::max(kr.begin(), std::min(kr.end(), (K)k)));

  return decomposeTemplate(msg, keys);
}

template <typename K, typename V>
void SharedParameter<K,V>::process(Message* msg) {
  double req = msg->task.request();
  // if (!req && msg->task.type() == Task::REPLY)
  //   return;

  switch (getCall(*msg).cmd()) {
    typedef CallSharedPara Call;

    case Call::PUSH:
      if (req) setValue(msg);
      break;

    case Call::PULL:
      if (req) {
        Message re = *msg;
        // re.value.clear();
        std::swap(re.sender, re.recver);
        re.task.set_request(false);

        getValue(&re);

        sys_.queue(re);
        // do not let the system double reply it
        msg->replied = true;
      } else {
        setValue(msg);
      }
      break;

    case Call::PUSH_REPLICA:
      if (req) setReplica(msg);
      break;

    case Call::PULL_REPLICA: {
      auto range = Range<K>(msg->task.key_range());
      if (req) {
        Message re = *msg;
        std::swap(re.sender, re.recver);
        re.task.set_request(false);

        getReplica(range, &re);

        LL << re;
        sys_.queue(re);
        // do not let the system double reply it
        msg->replied = true;
      } else {
        if (range == myKeyRange()) {
          recoverFrom(msg);
        } else {
          setReplica(msg);
        }
      }
    } break;
    default:
      CHECK(false) << "unknow cmd: " << getCall(*msg).ShortDebugString();
  }
}



} // namespace PS
