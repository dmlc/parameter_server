#pragma once

#include "Eigen/Dense"
#include "parameter/shared_parameter.h"

namespace PS {

// key-value vector, all keys are sorted and unique
template <typename K, typename V>
class KVVector : public SharedParameter<K,V> {
 public:
  // typedef Eigen::Matrix<V, Eigen::Dynamic, 1> EVec;
  using Customer::taskpool;
  using Customer::myNodeID;
  using SharedParameter<K,V>::getCall;
  using SharedParameter<K,V>::setCall;
  using SharedParameter<K,V>::myKeyRange;
  using SharedParameter<K,V>::keyRange;
  using SharedParameter<K,V>::sync;

  // return as an eigen3 vector
  Eigen::Map<Eigen::Matrix<V, Eigen::Dynamic, 1>> vec() { return val_.vec(); }

  SArray<V> segment(const SizeR& local_range) {
    return val_.segment(local_range);
  }

  SArray<V>& value() { return val_; }

  // # of keys, or the length of the vector
  size_t size() const { return key_.size(); }

  // # of nnz entries
  size_t nnz() const { return val_.nnz(); }
  //   size_t ret = 0;
  //   for (auto v : val_) ret += (v != 0);
  //   return ret;
  // }

  // resize val_ into the size as key_,
  // void resizeValue() {
  //   val_.resize(key_.size());
  //   memset(val_.data(), 0, val_.size() * sizeof(V));
  // }

  /////////////// synchronization

  //////////// functions for applications
  // global keys
  SArray<K>& key() { return key_; }

  // mapping a global key range into local range
  SizeR localRange (const Range<K>& global_range) {
    return key_.findRange(global_range);
  }

  // push a list of values to servers, and then pull the accoding values back.
  // callback will be run only once after the pulling is done
  // template <class Fn, typename W>
  // void roundTripForWorker(
  //     int time,
  //     const Range<K>& range,
  //     const std::initializer_list<SArray<W>>& push_value,
  //     Fn callback) {
  //   std::vector<SArray<W>> vals;
  //   for (auto& val : push_value) vals.push_back(val);
  //   roundTripForWorker(time, range, vals, callback());
  // }

  typedef std::function<void(int)> Fn;
  void roundTripForWorker(
      int time,
      const Range<K>& range = Range<K>::all(),
      const SArrayList<V>& push_value = SArrayList<V>(),
      Fn callback = Fn()) {
    auto lr = localRange(range);
    Message push_msg, pull_msg;

    // time 0 : push
    push_msg.key = key_.segment(lr);
    for (auto& val : push_value) {
      if (val.size() == 0) break;
      CHECK_EQ(lr.size(), val.size());
      push_msg.value.push_back(SArray<char>(val));
    }
    time = sync(CallSharedPara::PUSH, kServerGroup, range, push_msg, time);

    // time 1 : servers do update

    // time 2 : pull
    pull_msg.key = key_.segment(lr);
    sync(CallSharedPara::PULL, kServerGroup, range, pull_msg, time+2,
         time+1, []{}, [time, callback]{ callback(time+2); });
  }

  // aggregate the data from all clients, call update
  void roundTripForServer(
      int time,
      const Range<K>& range = Range<K>::all(),
      Fn update = Fn()) {
    auto wk = taskpool(kWorkerGroup);
    // none of my bussiness
    if (!key_.empty() && range.setIntersection(key_.range()).empty()) {
      // cl->finishInTask(cl->incrClock(3));
      return;
    }

    wk->waitIncomingTask(time);
    update(time);
    backup(kWorkerGroup, time+1, range);

    // time 1: mark it as finished so that all blocked pulls can be started
    wk->finishIncomingTask(time+1);
  }

  // return the data received at time t, then *delete* it
  AlignedArrayList<V> received(int t);

  // backup to replica nodes, will use the current time as the backup time,
  // which means, if you modified the data, you should call this function before
  // any other push and pull have been called. this function will increment the
  // clock by 1,
  void backup(const NodeID& sender, int time, const Range<K>& range) {
    if (FLAGS_num_replicas == 0) return;

    // data
    auto local_range = localRange(range);
    Message msg;
    msg.key = key_.segment(local_range);
    msg.addValue(val_.segment(local_range));

    // timestamp
    auto arg = setCall(&msg);
    auto vc = arg->add_backup();
    vc->set_time(time);
    vc->set_sender(sender);

    time = sync(CallSharedPara::PUSH_REPLICA, kReplicaGroup, range, msg);
    taskpool(kReplicaGroup)->waitOutgoingTask(time);

    // also save the timestamp in local, in case if my replica node dead, he
    // will request theose timestampes
    replica_[myKeyRange()].addTime(*vc);
  }

  ///////////////// function for the system
  std::vector<Message> decomposeTemplate(
      const Message& msg, const std::vector<K>& partition);

  void getValue(Message* msg);
  void setValue(Message* msg);

  void getReplica(Range<K> range, Message *msg) {
    // Range<K> kr(sp_->getCall(*msg).key());
    // auto kr = sp_->keyRange(msg->recver);
    auto it = replica_.find(range);
    CHECK(it != replica_.end())
        << myNodeID() << " does not backup for " << msg->sender
        << " with key range " << range;

    // data
    const auto& rep = it->second;
    if (range == myKeyRange()) {
      msg->key = key_;
      msg->addValue(val_);
    } else {
      msg->key = rep.key;
      msg->addValue(rep.value);
    }

    // timestamp
    auto arg = setCall(msg);
    for (auto& v : rep.clock) {
      auto sv = arg->add_backup();
      sv->set_sender(v.first);
      sv->set_time(v.second);
    }
  }

  void setReplica(Message *msg) {
    auto recved_key = SArray<K>(msg->key);
    auto recved_val = SArray<V>(msg->value[0]);
    auto kr = keyRange(msg->sender);
    auto it = replica_.find(kr);

    // data
    if (it == replica_.end()) {
      replica_[kr].key = recved_key;
      replica_[kr].value = recved_val;
      it = replica_.find(kr);
    } else {
      size_t n;
      auto aligned = match(
          it->second.key, recved_key, recved_val.data(), recved_key.range(), &n);
      CHECK_EQ(n, recved_key.size()) << "my key: " << it->second.key
                                     << "\n received key " << recved_key;
      it->second.value.segment(aligned.first).eigenVector() = aligned.second.eigenVector();
    }

    // LL << myNodeID() << " backup for " << msg->sender << " W: "
    //    << norm(replica_[kr].value.vec(), 1) << ", "
    //    << norm(recved_val.vec(), 1) << " " << recved_val.size();

    // timestamp
    auto arg = getCall(*msg);
    for (int i = 0; i < arg.backup_size(); ++i)
      it->second.addTime(arg.backup(i));
  }

  void recoverFrom(Message *msg) {
    // data
    auto arg = getCall(*msg);
    Range<K> range(msg->task.key_range());
    CHECK_EQ(range, myKeyRange());
    key_ = msg->key;
    val_ = msg->value[0];

    LL << myNodeID() << " has recovered W from " << msg->sender << " "
       << key_.size(); // << " keys with |W|_1 " << norm(vec(), 1);

    // timestamp
    for (int i = 0; i < arg.backup_size(); ++i) {
      auto w = taskpool(arg.backup(i).sender());
      w->finishIncomingTask(arg.backup(i).time());
      // LL << "replay " << arg.backup(i).sender() << " time " << arg.backup(i).time();
    }
  }

  // void recover() { sp_->recover(Range<K>(sp_->exec().node().key())); }
 private:
  SArray<K> key_;
  SArray<V> val_;

  std::unordered_map<int, AlignedArrayList<V> > recved_val_;
  std::mutex recved_val_mu_;

  struct Replica {
    SArray<K> key;
    SArray<V> value;
    std::vector<std::pair<NodeID, int> > clock;
    void addTime(const Timestamp& t) {
      clock.push_back(std::make_pair(t.sender(), t.time()));
    }
  };
  std::unordered_map<Range<K>, Replica> replica_;

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
void KVVector<K,V>::getValue(Message* msg) {
  // if (msg->key.empty()) return;
  CHECK_EQ(key_.size(), val_.size());
  SArray<K> recv_key(msg->key);
  size_t n = 0;
  auto aligned = match(recv_key, key_, val_.data(), key_.range(), &n);
  // LL << "val: " << dbstr(val_.data(), val_.size());
  CHECK_EQ(aligned.second.size(), recv_key.size());
  CHECK_EQ(recv_key.size(), n);
  msg->value.push_back(SArray<char>(aligned.second));
}

template <typename K, typename V>
void KVVector<K,V>::setValue(Message* msg) {
  Lock l(recved_val_mu_);
  SArray<K> recv_key(msg->key);
  Range<K> key_range(msg->task.key_range());

  if (msg->value.empty() && !msg->key.empty()) {
    key_ = key_.setUnion(recv_key);
    // LL << key_.size();
    return;
  }

  int t = msg->task.time();
  bool first = recved_val_.count(t) == 0;

  // LL << "recv push" << msg->debugString();
  for (int i = 0; i < msg->value.size(); ++i) {
    SArray<V> recv_data(msg->value[i]);
    CHECK_EQ(recv_data.size(), recv_key.size());
    size_t n = 0;
    auto aligned = match(key_, recv_key, recv_data.data(), key_range, &n);
    CHECK_GE(aligned.second.size(), recv_key.size());
    CHECK_EQ(recv_key.size(), n);

    if (first) {
      recved_val_[t].push_back(aligned);
    } else {
      CHECK_EQ(aligned.first, recved_val_[t][i].first);
      recved_val_[t][i].second.eigenArray() += aligned.second.eigenArray();
    }
  }
}

// partition is a sorted key ranges
template <typename K, typename V>
std::vector<Message> KVVector<K,V>::
decomposeTemplate(const Message& msg, const std::vector<K>& partition) {
  size_t n = partition.size();
  std::vector<size_t> pos(n, -1);

  SArray<K> key(msg.key);
  pos.reserve(n);
  for (auto k : partition)
    pos.push_back(std::lower_bound(key.begin(), key.end(), k) - key.begin());

  // split the message
  Message part = msg;
  std::vector<Message> ret(n-1);
  for (int i = 0; i < n-1; ++i) {
    part.clearData();
    if (Range<K>(partition[i], partition[i+1]).setIntersection(
            Range<K>(msg.task.key_range())).empty()) {
      // mark as do_not_send, because the remote node does not maintain this key range
      part.valid = false;
      // LL << myNodeID() << " " << part.shortDebugString() << " " << i;
    } else {
      part.valid = true;
      if (pos[i] == -1)
        pos[i] = std::lower_bound(key.begin(), key.end(), partition[i]) - key.begin();
      if (pos[i+1] == -1)
        pos[i+1] = std::lower_bound(key.begin(), key.end(), partition[i+1]) - key.begin();
      SizeR lr(pos[i], pos[i+1]);
      part.key = key.segment(lr);
      for (auto& d : msg.value) {
        SArray<V> data(d);
        part.value.push_back(SArray<char>(data.segment(lr)));
      }
    }
    ret[i] = part;
    // LL << ret[i].debugString();
  }
  return ret;
}


} // namespace PS
