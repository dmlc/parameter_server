#pragma once
#include "system/customer.h"
namespace PS {
class App : public Customer {
 public:
  App(const string& name) : Customer(name) { }
  virtual ~App() { }

  static App* create(const std::string& name, const std::string& conf);

  virtual void init() { }

  // run() is executed after all nodes have been executed init()
  virtual void run() { }

};

} // namespace PS

  // factory function, implemented in src/main.cc
  // static App* create(const AppConfig& config);
  // // TODO rename to app_conf_
  // AppConfig app_cf_;
