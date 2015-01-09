#pragma once
#include "system/customer.h"
#include "system/postoffice.h"
#include "proto/app.pb.h"

namespace PS {

class App : public Customer {
 public:
  App(const string& name) : Customer(name) { }
  virtual ~App() { }
  // factory function
  static App* create(const AppConfig& config);
  // initialization
  virtual void init() = 0;
  // run the applications: load data, iterating...
  virtual void run() = 0;

  // call by the scheduler
  // void stopAll();
  // void startSystem();

 protected:
  AppConfig app_cf_;

  // shut down server S0, and evoke R0 to replace S0
  // void testFaultTolerance(Task recover);
};

} // namespace PS
