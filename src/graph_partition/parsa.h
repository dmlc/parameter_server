#pragma once
#include "graph_partition/double_linked_array.h"
#include "graph_partition/parsa.pb.h"
#include "util/threadsafe_limited_queue.h"

namespace PS {

typedef float Empty;
typedef SparseMatrixPtr<int32, Empty> Graph;

class Parsa {
 public:
  void partitionU(const Graph& row_major_blk, const Graph& col_major_blk);
 private:
  std::vector<DblinkArray> cost_;

  struct NeighborSet {
    Bitmap assigned_V;
  };
  Bitmap assigned_U_;
  std::vector<NeighborSet> neighbor_set_;
  // number of partitions
  int k_;
  ParsaConf conf_;
  read_data_finished_ = false;


  unique_ptr<std::thread> data_thr_;
  bool read_data_finished_ = false;
  threadsafeLimitedQueue<MatrixPtrList<Empty> > data_buf_;
};
} // namespace PS
