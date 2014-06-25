#pragma once
#include "system/customer.h"
#include "system/postoffice.h"
#include "proto/config.pb.h"

namespace PS {

DECLARE_bool(test_fault_tol);

class App;
typedef std::shared_ptr<App> AppPtr;

class App : public Customer {
 public:

  // factory function
  static AppPtr create(const AppConfig& config);

  virtual void init() = 0;
  // run the applications: load data, iterating...
  virtual void run() = 0;

  void stop();

 private:
  void set(const AppConfig& config) {
    app_cf_ = config;
    CHECK(config.has_app_name());
    name_ = config.app_name();
    for (int i = 0; i < config.parameter_name_size(); ++i)
      child_customers_.push_back(config.parameter_name(i));
  }

 protected:
  AppConfig app_cf_;
  Timer busy_timer_;
  Timer total_timer_;

  // load nodes from a file, will be called only by the scheduler node
  void requestNodes();

  // shut down server S0, and evoke R0 to replace S0
  void testFaultTolerance(Task recover);

  // all nodes for this application
  std::unordered_map<NodeID, Node> nodes_;
};

} // namespace PS
