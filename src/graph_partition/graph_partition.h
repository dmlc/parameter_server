#pragma once
#include "system/app.h"
namespace PS {
namespace GP {
class GraphPartition : public App {
 public:
  static AppPtr create(const Config& conf);
  virtual void init() {
    conf_ = app_cf_.graph_partition();
  }
  virtual void run() { }
  virtual void process(const MessagePtr& msg) { }
 protected:
  Config conf_;
};

} // namespace GP
} // namespace PS
