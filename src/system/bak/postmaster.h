#pragma once
#include "util/common.h"
#include "util/range.h"
#include "system/proto/node.pb.h"
// #include "system/customer.h"
#include "data/proto/data.pb.h"
namespace PS {

class Postmaster {
 public:
  // Postmaster(Customer* obj) : obj_(CHECK_NOTNULL(obj)) { }
  Postmaster() { }

  static std::vector<DataConfig> partitionData(
      const DataConfig& conf, int num_workers);

  static std::vector<Node> partitionServerKeyRange(
      const std::vector<Node>& nodes, Range<Key> range);
  static std::vector<Node> assignNodeRank(const std::vector<Node>& nodes);

 private:
};

} // namespace PS
