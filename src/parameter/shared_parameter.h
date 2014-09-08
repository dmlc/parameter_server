#pragma once
#include "system/customer.h"
namespace PS {

#define USING_SHARED_PARAMETER                  \
  using Customer::taskpool;                     \
  using Customer::myNodeID;                     \
  using SharedParameter<K,V>::getCall;          \
  using SharedParameter<K,V>::setCall;          \
  using SharedParameter<K,V>::myKeyRange;       \
  using SharedParameter<K,V>::keyRange;         \
  using SharedParameter<K,V>::sync

// the base class of shared parameters
template <typename K, typename V>
class SharedParameter : public Customer {
 public:
  // Submit a task into *dest* and return the timestamp associated with this
  // task. If *time* != -1, then will set the timestamp of this task into *time*
  // rather than an auto-generated time. *wait_time* is the timestamp of the
  // task this submitted task must wait. No wait if it is -1.  *recv_handle*
  // will be called if anythings goes back from the destination node. When
  // called, this task has not been marked as finished. If could be called
  // multiple time when the destination node is a node group.  *fin_handle*
  // will be called when this task has been finished. If the dest node is a node
  // group, then it means replies from all nodes in this group have been
  // received.
  typedef std::function<void()> Fn;
  int sync(CallSharedPara_Command cmd, const NodeID& dest, Range<K> key_range,
           Message msg, int time = -1, int wait_time = -1, Fn recv_handle = Fn(),
           Fn fin_handle = Fn(), bool no_wait = true) {
    setCall(&(msg.task))->set_cmd(cmd);
    key_range.to(msg.task.mutable_key_range());
    if (time >= 0) msg.task.set_time(time);
    msg.task.set_wait_time(wait_time);
    return taskpool(dest)->submit(msg, recv_handle, fin_handle, no_wait);
  }
  // see *sync*
  int pull(const NodeID& dest, Range<K> key_range, Message data,
           int time = -1, int wait_time = -1,
           Fn recv_handle = Fn(), Fn fin_handle = Fn(), bool no_wait = true) {
    return sync(CallSharedPara::PULL, dest, key_range, data, time, wait_time,
                recv_handle, fin_handle, no_wait);
  }
  // see *sync*
  int push(const NodeID& dest, Range<K> key_range, Message data,
           int time = -1, int wait_time = -1,
           Fn recv_handle = Fn(), Fn fin_handle = Fn(), bool no_wait = true) {
    return sync(CallSharedPara::PUSH, dest, key_range, data, time, wait_time,
                recv_handle, fin_handle, no_wait);
  }
  CallSharedPara getCall(const Message& msg) {
    CHECK_EQ(msg.task.type(), Task::CALL_CUSTOMER);
    CHECK(msg.task.has_shared_para());
    return msg.task.shared_para();
  }
  CallSharedPara* setCall(Message *msg) {
    return setCall(&(msg->task));
  }
  // process a received message, will called by the thread of executor
  void process(Message* msg);
 protected:
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

  Range<K> myKeyRange() {
    return keyRange(Customer::myNodeID());
  }
  // query the key range of a node
  Range<K> keyRange(const NodeID& id) {
    return Range<K>(exec_.rnode(id)->keyRange());
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
void SharedParameter<K,V>::process(Message* msg) {
  double req = msg->task.request();
  switch (getCall(*msg).cmd()) {
    typedef CallSharedPara Call;
    case Call::PUSH:
      if (req) setValue(msg);
      break;
    case Call::PULL:
      if (req) {
        Message re = *msg;
        re.task.set_request(false);
        std::swap(re.sender, re.recver);
        getValue(&re);
        sys_.queue(taskpool(re.recver)->cacheKeySender(re));
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

        LL << re;  // TODO cache key?
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
