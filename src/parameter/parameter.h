#pragma once
#include "system/customer.h"
#include "parameter/proto/param.pb.h"
namespace PS {

// the base class of shared parameters
class Parameter : public Customer {
 public:
  Parameter(int id) : Customer(id)  { }
  virtual ~Parameter() { }

  typedef std::initializer_list<int> Timestamps;
  typedef std::initializer_list<FilterConfig> Filters;

  static Task Request(int channel,
                      const Timestamps& wait = {},
                      const Filters& filters = {},
                      const Range<Key>& key_range = Range<Key>::all()) {
    Task req; req.set_request(true);
    req.set_key_channel(channel);
    for (int t : wait) req.add_wait_time(t);
    for (const auto& f : filters) *req.add_filter() = f;
    key_range.to(req.mutable_key_range());
    return req;
  }


  inline int Push(Message* msg) {
    msg->task.mutable_param()->set_push(true);
    return Submit(msg);
  }

  inline int Pull(Message* msg) {
    msg->task.mutable_param()->set_push(false);
    return Submit(msg);
  }

  virtual void ProcessRequest(Message* request);
  virtual void ProcessResponse(Message* response);

 protected:
  // -- user-defined functions --

  // Fill "msg" with the values it requests, e.g.,
  //   msg->value(0)[0] = my_val_[msg->key[0]];
  virtual void GetValue(Message* msg) = 0;

  // set the values in "msg" into into my data strcuture, e.g..
  //  my_val_[msg->key[0]] = msg->value(0)[0];
  virtual void SetValue(Message* msg) = 0;

  // the message contains the backup KV pairs sent by the master node of the key
  // segment to its replica node. merge these pairs into my replica, say
  // replica_[msg->sender] = ...
  virtual void SetReplica(Message* msg) { }
  // retrieve the replica. a new server node replacing a dead server will first
  // ask for the dead's replica node for the data
  virtual void GetReplica(Message* msg) { }
  // a new server node fill its own datastructure via the the replica data from
  // the dead's replica node
  virtual void Recover(Message* msg) { }
};

}  // namespace PS
