#pragma once

#include "Eigen/Dense"
#include "parameter/shared_parameter.h"

namespace PS {

// key-value vector, the (global) keys are sorted and unique. Both keys and
// values are stored in arrays.
template <typename K, typename V>
class KVVector : public SharedParameter<K,V> {
 public:
  SArray<K>& key(int channel) { return key_[channel]; }
  SArray<V>& value(int channel) { return val_[channel]; }
  // find the local positions of a global key range

  SizeR find(int channel, const Range<K>& key_range) {
    return key_[channel].findRange(key_range);
  }


  // // slice a segment of the value using the local positions
  // SArray<V> slice(int channel, const SizeR& position) {
  //   return val_[channel].segment(local_range);
  // }

  // return the data received at time t, then *delete* it.
  AlignedArrayList<V> received(int t);

  // // # of keys, or the length of the vector
  // size_t size() const { return key_[0].size(); }
  // // # of nnz entries
  // size_t nnz() const { return val_[0].nnz(); }

  // implement the virtual functions required
  MessagePtrList slice(const MessagePtr& msg, const KeyList& sep);
  void getValue(const MessagePtr& msg);
  void setValue(const MessagePtr& msg);
  // TODO
  void setReplica(const MessagePtr& msg) { }
  void getReplica(const MessagePtr& msg) { }
  void recoverFrom(const MessagePtr& msg) { }
  USING_SHARED_PARAMETER;
 private:

  std::unordered_map<int, SArray<K>> key_;
  std::unordered_map<int, SArray<V>> val_;

  std::unordered_map<int, AlignedArrayList<V> > recved_val_;
  std::mutex recved_val_mu_;

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
};


template <typename K, typename V>
AlignedArrayList<V> KVVector<K, V>::received(int t) {
  Lock l(recved_val_mu_);
  auto it = recved_val_.find(t);
  CHECK(it != recved_val_.end()) << myNodeID() << " hasn't received data at time " << t;
  auto ret = it->second;
  recved_val_.erase(it);
  return ret;
}

template <typename K, typename V>
void KVVector<K,V>::setValue(const MessagePtr& msg) {
  // TODO review this logic. if received an empty message at time t, then call
  // received(t) will get an error
  int chl = msg->task.key_channel();
  // only keys, insert them
  SArray<K> recv_key(msg->key); if (recv_key.empty()) return;
  if (msg->value.empty()) {
    key_[chl] = key_[chl].setUnion(recv_key);
    val_[chl].clear();
    return;
  }
  // merge values, and store them in recved_val
  int t = msg->task.time();
  for (int i = 0; i < msg->value.size(); ++i) {
    SArray<V> recv_data(msg->value[i]);
    CHECK_EQ(recv_data.size(), recv_key.size());
    size_t n = 0;
    Range<K> key_range(msg->task.key_range());
    auto aligned = match(key_[chl], recv_key, recv_data.data(), key_range, &n);
    CHECK_GE(aligned.second.size(), recv_key.size()) << recv_key;
    CHECK_EQ(recv_key.size(), n);
    {
      Lock l(recved_val_mu_);
      if (recved_val_[t].size() <= i) {
        recved_val_[t].push_back(aligned);
      } else {
        // LL << t << " "<< i << " "  << aligned.first;
        CHECK_EQ(aligned.first, recved_val_[t][i].first);
        recved_val_[t][i].second.eigenArray() += aligned.second.eigenArray();
      }
    }
  }
}

template <typename K, typename V>
void KVVector<K,V>::getValue(const MessagePtr& msg) {
  SArray<K> recv_key(msg->key);
  if (recv_key.empty()) return;
  int ch = msg->task.key_channel();
  CHECK_EQ(key_[ch].size(), val_[ch].size());
  size_t n = 0;
  Range<Key> range = recv_key.range().setUnion(key_[ch].range());
  auto aligned = match(recv_key, key_[ch], val_[ch].data(), range, &n);
  CHECK_EQ(aligned.second.size(), recv_key.size()) << recv_key << "\n" << key_[ch];
  CHECK_GE(aligned.second.size(), n);
  msg->addValue(aligned.second);
}


// partition is a sorted key ranges
template <typename K, typename V>
MessagePtrList KVVector<K,V>::slice(const MessagePtr& msg, const KeyList& sep) {
  if (get(msg).replica()) return Customer::slice(msg, sep);

  // find the positions in msg.key
  size_t n = sep.size();
  std::vector<size_t> pos; pos.reserve(n-1);
  SArray<K> key(msg->key);
  Range<K> msg_key_range(msg->task.key_range());
  for (auto p : sep) {
    K k = std::max(msg_key_range.begin(), std::min(msg_key_range.end(), (K)p));
    pos.push_back(std::lower_bound(key.begin(), key.end(), k) - key.begin());
  }

  // split the message according to *pos*
  MessagePtrList ret(n-1);
  for (int i = 0; i < n-1; ++i) {
    MessagePtr piece(new Message(*msg));
    if (Range<K>(sep[i], sep[i+1]).setIntersection(msg_key_range).empty()) {
      // the remote node does not maintain this key range. mark this message as
      // valid, which will be not actually sent
      piece->valid = false;
    } else {
      piece->valid = true;  // must set true, otherwise this piece might not be sent
      piece->clearKV();
      if (!key.empty()) {  // void be divided by 0
        SizeR lr(pos[i], pos[i+1]);
        piece->key = key.segment(lr);
        for (auto& val : msg->value) {
          piece->addValue(val.segment(lr*(val.size()/key.size())));
        }
      }
    }
    ret[i] = piece;
  }
  return ret;
}


} // namespace PS

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
