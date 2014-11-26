#pragma once
#include "proto/example.pb.h"
#include "graph_partition/double_linked_array.h"
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
    if (get(msg).cmd() == Call::PARTITION_U_STAGE_0) {
      stage0();
    } else if (get(msg).cmd() == Call::PARTITION_U_STAGE_1) {
      stage1();
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

  void partitionU();
  void partitionU(const BlockData& blk, SArray<int>* map_U);
  void initCost(const GraphPtr& row_major_blk);
  void updateCostAndNeighborSet(
    const GraphPtr& row_major_blk, const GraphPtr& col_major_blk,
    const SArray<Key>& global_key, int Ui, int partition);
  void initNeighborSet(const SArray<uint64>& nbset);
  void sendUpdatedNeighborSet(int blk);

 private:
  typedef uint8 P;  //

// #ifdef EXACT_NBSET
//   std::vector<std::unordered_set<Key>> neighbor_set_;
// #else
//   std::vector<BlockBloomFilter<Key>> neighbor_set_;
// #endif
  std::vector<Bitmap> neighbor_set_;

  typedef std::pair<Key, P> KP;
  SArray<KP> added_neighbor_set_;
  SArray<Key> added_nbset_key_;
  SArray<uint64> added_nbset_value_;
  bool delta_nbset_;

  // about U
  std::vector<PARSA::DblinkArray> cost_;
  Bitmap assigned_U_;

  KVVectorPtr<Key, uint64> sync_nbset_;
  bool no_sync_;


  int num_partitions_;
  bool random_partition_;
};

} // namespace GP
} // namespace PS
