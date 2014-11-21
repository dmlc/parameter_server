#pragma once
#include "proto/example.pb.h"
#include "graph_partition/double_linked_array.h"
#include "graph_partition/graph_partition.h"
#include "util/producer_consumer.h"
#include "base/sparse_matrix.h"
#include "base/bitmap.h"
#include "base/block_bloom_filter.h"
#include "util/recordio.h"
#include "parameter/kv_vector.h"
namespace PS {
namespace GP {

#define EXACT_NBSET
typedef float Empty;
typedef SparseMatrix<uint32, Empty> Graph;
typedef std::shared_ptr<Graph> GraphPtr;
typedef std::vector<GraphPtr> GraphPtrList;
typedef std::vector<Example> ExampleList;
typedef std::shared_ptr<ExampleList> ExampleListPtr;

class ParsaWorker : public GraphPartition {
 public:
  virtual void init();
  void partition();
 private:
  struct BlockData {
    GraphPtr row_major;
    GraphPtr col_major;
    // SArray<Key> global_key;
    ExampleListPtr examples;
    int pull_time;
    int blk_id;
  };

  void partitionU(const BlockData& blk, SArray<int>* map_U);
  void initCost(const GraphPtr& row_major_blk, const SArray<Key>& global_key);
  void updateCostAndNeighborSet(
    const GraphPtr& row_major_blk, const GraphPtr& col_major_blk,
    const SArray<Key>& global_key, int Ui, int partition);
  void initNeighborSet(
      const SArray<Key>& global_key, const SArray<uint64>& nbset);
  void sendUpdatedNeighborSet(int blk);

 private:
  typedef uint8 P;  //

#ifdef EXACT_NBSET
  std::vector<std::unordered_set<Key>> neighbor_set_;
#else
  std::vector<BlockBloomFilter<Key>> neighbor_set_;
#endif

  typedef std::pair<Key, P> KP;
  SArray<KP> added_neighbor_set_;

  // about U
  std::vector<PARSA::DblinkArray> cost_;
  Bitmap assigned_U_;

  KVVector<Key, uint64> sync_nbset_;

  int num_partitions_;
};

} // namespace GP
} // namespace PS
