#pragma once
#include "graph_partition/double_linked_array.h"
#include "proto/parsa.pb.h"
#include "util/threadsafe_limited_queue.h"
#include "base/sparse_matrix.h"
#include "base/bitmap.h"
#include "system/app.h"

namespace PS {

typedef float Empty;
typedef SparseMatrix<uint32, Empty> Graph;
typedef std::shared_ptr<Graph> GraphPtr;
typedef std::vector<GraphPtr> GraphPtrList;
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

  void loadInputGraphPtr();

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
  bool read_data_finished_ = false;


  unique_ptr<std::thread> data_thr_;
  threadsafeLimitedQueue<GraphPtrList> data_buf_;
};
} // namespace PS
