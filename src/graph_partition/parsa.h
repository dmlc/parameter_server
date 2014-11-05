#pragma once
#include "graph_partition/double_linked_array.h"
namespace PS {

typedef SparseMatrixPtr<int32, float> Graph;

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
};
} // namespace PS
