#pragma once
#include "ps.h"
#include "parameter/proto/param.pb.h"
namespace PS {

// the base class of shared parameters
class Parameter : public Customer {
 public:
  Parameter(int id = NextCustomerID()) : Customer(id)  { }
  virtual ~Parameter() { }

  int Push(MessagePtr& msg) {
    msg->task.mutable_param()->set_cmd(ParamCall::PUSH);
    return Submit(msg);
  }

  int Pull(MessagePtr& msg) {
    msg->task.mutable_param()->set_cmd(ParamCall::PULL);
    return Submit(msg);
  }

  virtual
 protected:

};
}  // namespace PS
