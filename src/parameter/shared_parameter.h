#pragma once
#include "system/customer.h"
#include "parameter/frequency_filter.h"
namespace PS {

#define USING_SHARED_PARAMETER                  \
  using Customer::taskpool;                     \
  using Customer::myNodeID;                     \
  using SharedParameter<K,V>::get;              \
  using SharedParameter<K,V>::set;              \
  using SharedParameter<K,V>::myKeyRange;       \
  using SharedParameter<K,V>::keyRange;         \
  using SharedParameter<K,V>::sync

// the base class of shared parameters
template <typename K, typename V>
class SharedParameter : public Customer {
 public:
  // convenient wrappers of functions in remote_node.h
  int sync(MessagePtr msg) {
    CHECK(msg->task.shared_para().has_cmd()) << msg->debugString();
    if (!msg->task.has_key_range()) Range<K>::all().to(msg->task.mutable_key_range());
    return taskpool(msg->recver)->submit(msg);
  }
  int push(MessagePtr msg) {
    set(msg)->set_cmd(CallSharedPara::PUSH);
    return sync(msg);
  }
  int pull(MessagePtr msg) {
    set(msg)->set_cmd(CallSharedPara::PULL);
    return sync(msg);
  }
  void waitInMsg(const NodeID& node, int time) {
    taskpool(node)->waitIncomingTask(time);
  }

  void waitOutMsg(const NodeID& node, int time) {
    taskpool(node)->waitOutgoingTask(time);
  }

  void finish(const NodeID& node, int time) {
    taskpool(node)->finishIncomingTask(time);
  }

  // process a received message, will called by the thread of executor
  void process(const MessagePtr& msg);

  CallSharedPara* set(MessagePtr msg) {
    msg->task.set_type(Task::CALL_CUSTOMER);
    return msg->task.mutable_shared_para();
  }
  CallSharedPara get(const MessagePtr& msg) {
    CHECK_EQ(msg->task.type(), Task::CALL_CUSTOMER);
    CHECK(msg->task.has_shared_para());
    return msg->task.shared_para();
  }
 protected:
  // fill the values specified by the key lists in msg
  virtual void getValue(const MessagePtr& msg) = 0;
  // set the received KV pairs into my data strcuture
  virtual void setValue(const MessagePtr& msg) = 0;
  // the message contains the backup KV pairs sent by the master node of the key
  // segment to its replica node. merge these pairs into my replica, say
  // replica_[msg->sender] = ...
  virtual void setReplica(const MessagePtr& msg) = 0; //
  // retrieve the replica. a new server node replacing a dead server will first
  // ask for the dead's replica node for the data
  virtual void getReplica(const MessagePtr& msg)  = 0;
  // a new server node fill its own datastructure via the the replica data from
  // the dead's replica node
  virtual void recoverFrom(const MessagePtr& msg) = 0; //
  // recover from a replica node
  void recover(Range<K> range);

  Range<K> myKeyRange() {
    return keyRange(Customer::myNodeID());
  }
  // query the key range of a node
  Range<K> keyRange(const NodeID& id) {
    return Range<K>(exec_.rnode(id)->keyRange());
  }
 private:
  std::unordered_map<int, FreqencyFilter<K>> key_filter_;

  // add key_range in the future, it is not necessary now
  std::unordered_map<NodeID, std::vector<int> > clock_replica_;
};

template <typename K, typename V>
void SharedParameter<K,V>::process(const MessagePtr& msg) {
  bool req = msg->task.request();
  int chl = msg->task.key_channel();
  auto call = get(msg);
  bool push = call.cmd() == CallSharedPara::PUSH;
  bool pull = call.cmd() == CallSharedPara::PULL;
  MessagePtr reply;
  if (pull && req) {
    reply = MessagePtr(new Message(*msg));
    reply->task.set_request(false);
    std::swap(reply->sender, reply->recver);
  }
  // process
  if (call.replica()) {
    if (pull && !req && Range<K>(msg->task.key_range()) == myKeyRange()) {
      recoverFrom(msg);
    } else if ((push && req) || (pull && !req)) {
      setReplica(msg);
    } else if (pull && req) {
      getReplica(reply);
    }
  } else if (call.insert_key_freq()) {
    if (push && req && !msg->value.empty()) {
      key_filter_[chl].insertKeys(
          SArray<K>(msg->key), SArray<uint32>(msg->value[0]),
          call.countmin_n(), call.countmin_k());
    }
  } else if (call.has_query_key_freq()) {
    if (pull && req) {
      reply->key = key_filter_[chl].queryKeys(
          SArray<K>(msg->key), call.query_key_freq());
      // LL << reply->key.size();
    } else if (pull && !req) {
      setValue(msg);
      // LL << msg->key.size();
    }
  } else {
    if ((push && req) || (pull && !req)) {
      setValue(msg);
    } else if (pull && req) {
      getValue(reply);
    }
  }
  // reply if necessary
  if (pull && req) {
    taskpool(reply->recver)->cacheKeySender(reply);
    sys_.queue(reply);
    msg->replied = true;
  }
}

// template <typename K, typename V>
// void SharedParameter<K,V>::recover(Range<K> range) {
//   // TODO recover from checkpoint

//   // CHECK_GT(FLAGS_num_replicas, 0);
//   // Task task;
//   // task.set_type(Task::CALL_CUSTOMER);
//   // auto arg = task.mutable_shared_para();
//   // arg->set_cmd(CallSharedPara::PULL_REPLICA);
//   // range.to(arg->mutable_key());

//   // auto slave = exec_.group(kReplicaGroup)[0];
//   // slave->submitAndWait(task);

//   // for (auto owner : exec_.group(kOwnerGroup)) {
//   //   LL << "ask for " << owner->id() << " for replica";
//   //   keyRange(owner->id()).to(arg->mutable_key());
//   //   owner->submitAndWait(task);
//   //   LL << "done";
//   // }
// }




} // namespace PS
