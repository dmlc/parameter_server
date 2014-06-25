#pragma once

#include "system/message.h"

namespace PS {

template <typename K, typename V>
class SharedParameter;

// an KV storage interface, defines functions required by SharedParameter, see
// SharedParameter<K,V>::process(Message* msg)
// msg is a message received from a remote node
template <typename K, typename V>
class KVStore {
 public:
  KVStore() {}
  ~KVStore() {}

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

  virtual std::vector<Message> decompose(
      const Message& msg, const std::vector<K>& partition) = 0;

  // recover from a replica node
  // TODO recover from a checkpoint
  virtual void recover() = 0;

 protected:
 // protected:
  // std::unique_ptr<SharedParameter<K,V> > sp_;
};

} // namespace PS
