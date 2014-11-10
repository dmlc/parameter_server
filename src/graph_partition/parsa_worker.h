#pragma once
#include "proto/example.pb.h"
#include "graph_partition/double_linked_array.h"
#include "graph_partition/parsa.pb.h"
#include "util/producer_consumer.h"
#include "base/sparse_matrix.h"
#include "base/bitmap.h"
#include "base/block_bloom_filter.h"
#include "util/recordio.h"
namespace PS {
#define EXACT_NBSET

typedef float Empty;
typedef SparseMatrix<uint32, Empty> Graph;
typedef std::shared_ptr<Graph> GraphPtr;
typedef std::vector<GraphPtr> GraphPtrList;
typedef std::vector<Example> ExampleList;
typedef std::shared_ptr<ExampleList> ExampleListPtr;


class ParsaWorker {
 public:
  ParsaWorker(const ParsaConf& conf);
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
  void initWorkerNbset(
      const SArray<Key>& global_key, const SArray<uint64>& nbset);
  void sendWorkerUpdatedNbset();

 private:
  typedef uint8 P;  //
  typedef uint64 S;
  // the neighbor sets of U
  std::unordered_map<Key, S> server_nbset_;
#ifdef EXACT_NBSET
  std::vector<std::unordered_set<Key>> worker_nbset_;
#else
  std::vector<BlockBloomFilter<Key>> worker_nbset_;
#endif

  typedef std::pair<Key, P> KP;
  SArray<KP> worker_added_nbset_;

  // about U
  std::vector<PARSA::DblinkArray> cost_;
  Bitmap assigned_U_;

  int num_partitions_;
  ParsaConf conf_;

  // std::vector<int16> partition_V_;
  std::unordered_map<Key, P> partition_V_;
};
} // namespace PS
