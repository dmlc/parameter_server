#pragma once
#include "util/common.h"
#include "system/proto/node.pb.h"
#include "system/customer.h"
namespace PS {

class Postmaster {
 public:
  Postmaster(Customer* obj) : obj_(CHECK_NOTNULL(obj)) { }

  static std::vector<DataConfig> partitionData(
      const DataConfig& conf, int num_workers);

  static std::vector<Node> partitionServerKeyRange(
      const std::vector<Node>& nodes, Range<Key> range);
  static std::vector<Node> assignNodeRank(const std::vector<Node>& nodes);
 private:
  Customer* obj_;
};

} // namespace PS
