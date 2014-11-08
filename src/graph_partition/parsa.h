#pragma once
#include "graph_partition/double_linked_array.h"
#include "proto/parsa.pb.h"
#include "util/producer_consumer.h"
#include "base/sparse_matrix.h"
#include "base/bitmap.h"
#include "system/app.h"
#include "util/recordio.h"

namespace PS {

typedef float Empty;
typedef SparseMatrix<uint32, Empty> Graph;
typedef std::shared_ptr<Graph> GraphPtr;
typedef std::vector<GraphPtr> GraphPtrList;
typedef std::vector<Example> ExampleList;
typedef std::shared_ptr<ExampleList> ExampleListPtr;

class Parsa : public App {
 public:
  void init();
  void run();
  void process(const MessagePtr& msg) { }
 private:
  void partition();
  void partitionV();
  void partitionU(
      const GraphPtr& row_major_blk, const GraphPtr& col_major_blk,
      const SArray<Key>& global_key, SArray<int>* map_U);
  void initCost(
      const GraphPtr& row_major_blk, const SArray<Key>& global_key);
  void updateCostAndNeighborSet(
      const GraphPtr& row_major_blk, const GraphPtr& col_major_blk,
      const SArray<Key>& global_key, int Ui, int partition);

  uint32 hash(uint64 key) { return key % num_V_; }

  std::vector<PARSA::DblinkArray> cost_;
  struct NeighborSet {
    // Bitmap assigned_V;
    std::unordered_set<Key> assigned_V;
  };
  Bitmap assigned_U_;
  std::vector<NeighborSet> neighbor_set_;
  // number of partitions
  int num_partitions_;
  int num_V_;
  ParsaConf conf_;

  // std::vector<int16> partition_V_;
  std::unordered_map<Key, uint8> partition_V_;

};
} // namespace PS
