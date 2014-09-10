#pragma once
#include "system/customer.h"
#include "system/postoffice.h"
#include "proto/app.pb.h"

namespace PS {

DECLARE_bool(test_fault_tol);

class App;
typedef std::shared_ptr<App> AppPtr;

class App : public Customer {
 public:
  // factory function
  static AppPtr create(const AppConfig& config);
  // initialization
  virtual void init() = 0;
  // run the applications: load data, iterating...
  virtual void run() = 0;
  void stop();
 private:
  void set(const AppConfig& config) {
    CHECK(config.has_app_name());
    app_cf_ = config;
    name_ = config.app_name();
    for (int i = 0; i < config.parameter_name_size(); ++i) {
      child_customers_.push_back(config.parameter_name(i));
    }
  }

 protected:
  AppConfig app_cf_;
  Timer busy_timer_;
  Timer total_timer_;

  // shut down server S0, and evoke R0 to replace S0
  void testFaultTolerance(Task recover);
};

} // namespace PS
