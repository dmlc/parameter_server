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
      const GraphPtr& row_major_blk, const GraphPtr& col_major_blk, SArray<int>* map_U);

  void initCost(const GraphPtr& row_major_blk);

  void updateCostAndNeighborSet(
      const GraphPtr& row_major_blk, const GraphPtr& col_major_blk, int Ui, int partition);

  std::vector<PARSA::DblinkArray> cost_;
  struct NeighborSet {
    Bitmap assigned_V;
  };
  Bitmap assigned_U_;
  std::vector<NeighborSet> neighbor_set_;
  // number of partitions
  int num_partitions_;
  int num_V_;
  ParsaConf conf_;


  typedef std::pair<ExampleListPtr, SArray<int>> ResultPair;
  ProducerConsumer<ResultPair> writer_1_;
  ProducerConsumer<ResultPair> writer_2_;

  std::vector<RecordWriter> proto_writers_1_;
};
} // namespace PS
