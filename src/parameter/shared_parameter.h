#pragma once
#include "system/customer.h"
#include "parameter/frequency_filter.h"
namespace PS {

template <typename K> class SharedParameter;
template <typename K> using SharedParameterPtr = std::shared_ptr<SharedParameter<K>>;

// the base class of shared parameters
template <typename K>
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

  FreqencyFilter<K>& keyFilter(int chl) { return key_filter_[chl]; }
  void setKeyFilterIgnoreChl(bool flag) { key_filter_ignore_chl_ = flag; }
  // void clearKeyFilter(int chl) { key_filter_[chl].clear(); }

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
  bool key_filter_ignore_chl_ = false;


  // add key_range in the future, it is not necessary now
  std::unordered_map<NodeID, std::vector<int> > clock_replica_;
};

template <typename K>
void SharedParameter<K>::process(const MessagePtr& msg) {
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

  this->sys_.hb().startTimer(HeartbeatInfo::TimerType::BUSY);
  // process
  if (call.replica()) {
    if (pull && !req && Range<K>(msg->task.key_range()) == myKeyRange()) {
      recoverFrom(msg);
    } else if ((push && req) || (pull && !req)) {
      setReplica(msg);
    } else if (pull && req) {
      getReplica(reply);
    }
  } else if (call.insert_key_freq() || call.has_query_key_freq()) {
    // deal with tail features
    if (key_filter_ignore_chl_) chl = 0;
    if (call.insert_key_freq() && req && !msg->value.empty()) {
      auto& filter = key_filter_[chl];
      if (filter.empty()) {
        double w = (double)FLAGS_num_workers;
        filter.resize(
            std::max((int)(w*call.countmin_n()/log(w+1)), 64), call.countmin_k());
      }
      filter.insertKeys(SArray<K>(msg->key), SArray<uint32>(msg->value[0]));
    }
    if (call.has_query_key_freq()) {
      if (req) {
        reply->clearKV();
        reply->key = key_filter_[chl].queryKeys(SArray<K>(msg->key), call.query_key_freq());
      } else {
        setValue(msg);
      }
    }
  } else {
    if ((push && req) || (pull && !req)) {
      setValue(msg);
    } else if (pull && req) {
      getValue(reply);
    }
  }
  this->sys_.hb().stopTimer(HeartbeatInfo::TimerType::BUSY);

  // reply if necessary
  if (pull && req) {
    taskpool(reply->recver)->cacheKeySender(reply);
    sys_.queue(reply);
    msg->replied = true;
  }
}

#define USING_SHARED_PARAMETER                  \
  using Customer::taskpool;                     \
  using Customer::myNodeID;                     \
  using SharedParameter<K>::get;                \
  using SharedParameter<K>::set;                \
  using SharedParameter<K>::myKeyRange;         \
  using SharedParameter<K>::keyRange;           \
  using SharedParameter<K>::sync


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
