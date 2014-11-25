#pragma once
#include "proto/node.pb.h"
#include "proto/app.pb.h"
#include "util/common.h"
#include "system/customer.h"
// class Customer;
namespace PS {

class Postmaster {
 public:
  Postmaster(Customer* obj) : obj_(CHECK_NOTNULL(obj)) { }

  std::vector<Node> nodes();
  void createApp(const std::vector<Node>& nodes, const std::vector<AppConfig>& apps);
  void stopApp();

  std::vector<DataConfig> partitionData(const DataConfig& conf, int num_workers);
  std::vector<Node> partitionKey(const std::vector<Node>& nodes, Range<Key> range);
 private:
  Customer* obj_;
};

} // namespace PS
