#pragma once
#include "proto/example.pb.h"
#include "graph_partition/double_linked_array.h"
#include "graph_partition/parsa_common.h"
#include "graph_partition/graph_partition.h"
#include "util/producer_consumer.h"
#include "data/stream_reader.h"
#include "base/sparse_matrix.h"
#include "base/bitmap.h"
#include "base/block_bloom_filter.h"
#include "util/recordio.h"
#include "parameter/kv_vector.h"
namespace PS {
namespace GP {

// #define EXACT_NBSET
typedef float Empty;
typedef SparseMatrix<uint32, Empty> Graph;
typedef std::shared_ptr<Graph> GraphPtr;
typedef std::vector<GraphPtr> GraphPtrList;
typedef std::vector<Example> ExampleList;
typedef std::shared_ptr<ExampleList> ExampleListPtr;

class ParsaWorker : public GraphPartition {
 public:
  virtual void init();
  virtual void process(const MessagePtr& msg) {
    auto cmd = get(msg).cmd();
    if (cmd == Call::PARTITION_U_STAGE_0) {
      stage0();
    } else if (cmd == Call::PARTITION_U_STAGE_1) {
      stage1();
    } else if (cmd == Call::PARTITION_V) {
      remapKey();
    }
  }
 private:
  struct BlockData {
    GraphPtr row_major;
    GraphPtr col_major;
    // SArray<Key> global_key;
    ExampleListPtr examples;
    int pull_time;
    int blk_id;
  };

  void readGraph(
      StreamReader<Empty>& stream, ProducerConsumer<BlockData>& producer,
      int& start_id, int end_id, int block_size, bool keep_examples);

  void stage0();
  void stage1();
  void remapKey();

  void partitionU(const BlockData& blk, SArray<int>* map_U);
  void initCost(const GraphPtr& row_major_blk);
  void updateCostAndNeighborSet(
    const GraphPtr& row_major_blk, const GraphPtr& col_major_blk,
    const SArray<Key>& global_key, int Ui, int partition);
  void initNeighborSet(const SArray<V>& nbset);
  void sendUpdatedNeighborSet(int blk);

 private:

// #ifdef EXACT_NBSET
//   std::vector<std::unordered_set<Key>> neighbor_set_;
// #else
//   std::vector<BlockBloomFilter<Key>> neighbor_set_;
// #endif
  std::vector<Bitmap> neighbor_set_;

  SArray<KP> added_neighbor_set_;
  SArray<Key> added_nbset_key_;
  SArray<V> added_nbset_value_;
  bool delta_nbset_;

  // about U
  std::vector<PARSA::DblinkArray> cost_;
  Bitmap assigned_U_;

  KVVectorPtr<Key, V> sync_nbset_;
  bool no_sync_;

  std::vector<int> push_time_;
  int num_partitions_;
  bool random_partition_;

  DataConfig tmp_files_;
};

} // namespace GP
} // namespace PS
