#pragma once
#include "system/customer.h"
#include "proto/app.pb.h"
namespace PS {

class App : public Customer {
 public:
  App(const string& name) : Customer(name) { }
  virtual ~App() { }

  // factory function, implemented in src/main.cc
  static App* create(const AppConfig& config);

  // initialization. it will be called in _create_. at that time, app_cf_
  // has been set, but this app may not have complete nodes information
  virtual void init() { }

  // run the applications, which is executed at the scheduler node. other nodes,
  // such as workers and servers, then run the workloads issued by the scheduler
  // or others. in other words, executor's thread calls _process_ to run
  // the workload.
  virtual void run() {
    CHECK_EQ(exec_.myNode().role(), Node::SCHEDULER)
        << "_run_ must be called at the scheduler";
    LL << "you must define your own _run_ function for the scheduler";
  }

 protected:
  // TODO rename to app_conf_
  AppConfig app_cf_;
};

} // namespace PS
