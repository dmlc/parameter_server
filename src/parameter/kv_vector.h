#pragma once
#include "Eigen/Dense"
#include "parameter/shared_parameter.h"
#include "util/parallel_ordered_match.h"
namespace PS {

template<typename K, typename V> class KVVector;
template<typename K, typename V> using KVVectorPtr = std::shared_ptr<KVVector<K,V>>;

// key-value vector, the (global) keys are sorted and unique. Both keys and
// values are stored in arrays.
template <typename K, typename V>
class KVVector : public SharedParameter<K> {
 public:
  USING_SHARED_PARAMETER;
  SArray<K>& key(int channel) { return key_[channel]; }
  SArray<V>& value(int channel) { return val_[channel]; }
  void clear(int channel) { key_.erase(channel); val_.erase(channel); }

  // find the local positions of a global key range
  SizeR find(int channel, const Range<K>& key_range) {
    return key_[channel].findRange(key_range);
  }

  // return the mareged data received at time t, then *delete* it. If no
  // message, or empty messages are received at time t, then call received(t)
  // will get an error
  typedef std::pair<SizeR, std::vector<SArray<V>>> MergedData;
  MergedData received(int t);

  // implement the virtual functions required
  MessagePtrList slice(const MessagePtr& msg, const KeyList& sep);
  void getValue(const MessagePtr& msg);
  void setValue(const MessagePtr& msg);

  // TODO
  void setReplica(const MessagePtr& msg) { }
  void getReplica(const MessagePtr& msg) { }
  void recoverFrom(const MessagePtr& msg) { }

 private:
  std::unordered_map<int, SArray<K>> key_;
  std::unordered_map<int, SArray<V>> val_;

  std::unordered_map<int, MergedData> recved_val_;
  std::mutex recved_val_mu_;
};


template <typename K, typename V>
typename KVVector<K,V>::MergedData KVVector<K,V>::received(int t) {
  Lock l(recved_val_mu_);
  auto it = recved_val_.find(t);
  CHECK(it != recved_val_.end()) << myNodeID() << " hasn't received data at time " << t;
  auto ret = it->second;
  recved_val_.erase(it);
  return ret;
}

template <typename K, typename V>
void KVVector<K,V>::setValue(const MessagePtr& msg) {
  SArray<K> recv_key(msg->key);
  if (recv_key.empty()) return;
  int chl = msg->task.key_channel();
  if (msg->value.size() == 0) {
    // only keys, merge these keys, and also clear the values
    key_[chl] = key_[chl].setUnion(recv_key);
    val_[chl].clear();
    return;
  }

  // merge values, and store them in recved_val
  int t = msg->task.time();
  Range<K> key_range(msg->task.key_range());
  SizeR idx_range = key_[chl].findRange(key_range);

  recved_val_mu_.lock();
  auto& matched = recved_val_[t];
  recved_val_mu_.unlock();

  for (int i = 0; i < msg->value.size(); ++i) {
    SArray<V> recv_data(msg->value[i]);
    CHECK_EQ(recv_data.size(), recv_key.size());
    bool first = matched.second.size() <= i;
    if (first) {
      // it is the first node, allocate the memory
      matched.first = idx_range;
      matched.second.push_back(SArray<V>());
      CHECK_EQ(parallelOrderedMatch(
          recv_key, recv_data, key_[chl].segment(idx_range),
          OpAssign<V>(), FLAGS_num_threads, &matched.second[i]), recv_key.size());
    } else {
      CHECK_EQ(matched.first, idx_range);
      CHECK_EQ(parallelOrderedMatch(
          recv_key, recv_data, key_[chl].segment(idx_range),
          OpPlus<V>(), FLAGS_num_threads, &matched.second[i]), recv_key.size());
    }
  }
}

template <typename K, typename V>
void KVVector<K,V>::getValue(const MessagePtr& msg) {
  SArray<K> recv_key(msg->key);
  if (recv_key.empty()) return;
  int chl = msg->task.key_channel();
  CHECK_EQ(key_[chl].size(), val_[chl].size());

  SArray<V> val;
  // auto op = [](const V* src, V* dst) { *dst = *src; };
  size_t n = parallelOrderedMatch(
      key(chl), value(chl), recv_key, OpAssign<V>(), FLAGS_num_threads, &val);
  CHECK_EQ(n, val.size());
  msg->clearValue();
  msg->addValue(val);
}

// partition is a sorted key ranges
template <typename K, typename V>
MessagePtrList KVVector<K,V>::slice(const MessagePtr& msg, const KeyList& sep) {
  if (get(msg).replica()) return Customer::slice(msg, sep);
  return sliceKeyOrderedMsg<K>(msg, sep);
}

} // namespace PS


// TODO
// struct Replica {
//   SArray<K> key;
//   SArray<V> value;
//   std::vector<std::pair<NodeID, int> > clock;
//   void addTime(const Timestamp& t) {
//     clock.push_back(std::make_pair(t.sender(), t.time()));
//   }
// };
// std::unordered_map<Range<K>, Replica> replica_;

// void setReplica(const MessagePtr& msg);
// void getReplica(const Range<K>& range, const MessagePtr& msg);
// void recoverFrom(const MessagePtr& msg);

// template <typename K, typename V>
// void KVVector<K, V>::getReplica(const Range<K>& range, const MessagePtr& msg) {
//   auto it = replica_.find(range);
//   CHECK(it != replica_.end())
//       << myNodeID() << " does not backup for " << msg->sender
//       << " with key range " << range;

//   // data
//   const auto& rep = it->second;
//   if (range == myKeyRange()) {
//     msg->key = key_;
//     msg->addValue(val_);
//   } else {
//     msg->key = rep.key;
//     msg->addValue(rep.value);
//   }

//   // timestamp
//   for (auto& v : rep.clock) {
//     auto sv = set(msg)->add_backup();
//     sv->set_sender(v.first);
//     sv->set_time(v.second);
//   }
// }

// template <typename K, typename V>
// void KVVector<K, V>::setReplica(const MessagePtr& msg) {
//   auto recved_key = SArray<K>(msg->key);
//   auto recved_val = SArray<V>(msg->value[0]);
//   auto kr = keyRange(msg->sender);
//   auto it = replica_.find(kr);

//   // data
//   if (it == replica_.end()) {
//     replica_[kr].key = recved_key;
//     replica_[kr].value = recved_val;
//     it = replica_.find(kr);
//   } else {
//     size_t n;
//     auto aligned = match(
//         it->second.key, recved_key, recved_val.data(), recved_key.range(), &n);
//     CHECK_EQ(n, recved_key.size()) << "my key: " << it->second.key
//                                    << "\n received key " << recved_key;
//     it->second.value.segment(aligned.first).eigenVector() = aligned.second.eigenVector();
//   }

//   // LL << myNodeID() << " backup for " << msg->sender << " W: "
//   //    << norm(replica_[kr].value.vec(), 1) << ", "
//   //    << norm(recved_val.vec(), 1) << " " << recved_val.size();

//   // timestamp
//   auto arg = get(msg);
//   for (int i = 0; i < arg.backup_size(); ++i) it->second.addTime(arg.backup(i));
// }

// template <typename K, typename V>
// void KVVector<K, V>::recoverFrom(const MessagePtr& msg) {
//   // data
//   auto arg = get(msg);
//   Range<K> range(msg->task.key_range());
//   CHECK_EQ(range, myKeyRange());
//   key_ = msg->key;
//   val_ = msg->value[0];

//   LL << myNodeID() << " has recovered W from " << msg->sender << " "
//      << key_.size(); // << " keys with |W|_1 " << norm(vec(), 1);

//   // timestamp
//   for (int i = 0; i < arg.backup_size(); ++i) {
//     auto w = taskpool(arg.backup(i).sender());
//     w->finishIncomingTask(arg.backup(i).time());
//     // LL << "replay " << arg.backup(i).sender() << " time " << arg.backup(i).time();
//   }
// }


// template <typename K, typename V>
// void KVVector<K, V>::backup(const NodeID& sender, int time, const Range<K>& range) {
//   // TODO
//   // if (FLAGS_num_replicas == 0) return;

//   // // data
//   // auto local_range = localRange(range);
//   // Message msg;
//   // msg.key = key_.segment(local_range);
//   // msg.addValue(val_.segment(local_range));

//   // // timestamp
//   // auto arg = setCall(&msg);
//   // auto vc = arg->add_backup();
//   // vc->set_time(time);
//   // vc->set_sender(sender);

//   // time = sync(CallSharedPara::PUSH_REPLICA, kReplicaGroup, range, msg);
//   // taskpool(kReplicaGroup)->waitOutgoingTask(time);

//   // // also save the timestamp in local, in case if my replica node dead, he
//   // // will request theose timestampes
//   // replica_[myKeyRange()].addTime(*vc);
// }

//   // fetch the values of *key()* from the servers
//   // void fetchValueFromServers();

//   // backup to replica nodes, will use the current time as the backup time,
//   // which means, if you modified the data, you should call this function before
//   // any other push and pull have been called. this function will increment the
//   // clock by 1,
//   // void backup(const NodeID& sender, int time, const Range<K>& range);

// template <typename K, typename V>
// void KVVector<K, V>::fetchValueFromServers() {
//   // Message pull_msg; pull_msg.key = key_;
//   // int time = sync(
//   //     CallSharedPara::PULL, kServerGroup, Range<Key>::all(), pull_msg);
//   // taskpool(kServerGroup)->waitOutgoingTask(time);
//   // auto recv = received(time);
//   // val_ = recv[0].second;
//   // CHECK_EQ(val_.size(), key_.size());
// }


  // size_t memSize() const {
  //   size_t mem = 0;
  //   for (const auto& it : key_) mem += it.second.memSize();
  //   for (const auto& it : val_) mem += it.second.memSize();
  //   for (const auto& it : recved_val_) {
  //     for (const auto& v : it.second) mem += v.second.memSize();
  //   }
  //   return mem;
  // }

  // // slice a segment of the value using the local positions
  // SArray<V> slice(int channel, const SizeR& position) {
  //   return val_[channel].segment(local_range);
  // }
  // // # of keys, or the length of the vector
  // size_t size() const { return key_[0].size(); }
  // // # of nnz entries
  // size_t nnz() const { return val_[0].nnz(); }
