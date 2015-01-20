#pragma once
#include "system/customer.h"
namespace PS {
class App : public Customer {
 public:
  App(const string& name = FLAGS_app_name) : Customer(name) { }
  virtual ~App() { }

  // factory functionn
  static App* create(const std::string& name, const std::string& conf);

  // init() is called after this app has been created at all nodes.
  virtual void init() { }

  // run() is executed after all nodes have been executed init()
  virtual void run() { }

};

} // namespace PS
