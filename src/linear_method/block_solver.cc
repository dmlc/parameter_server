#include "linear_method/block_solver.h"

namespace PS {
namespace LM {

BlockSolver::FeatureBlocks BlockSolver::partitionFeatures(
      const AppConfig& app_cf, const std::vector<MatrixInfo>& infos) {
  FeatureBlocks blocks;
  CHECK(app_cf.has_block_solver());
  auto cf = app_cf.block_solver();
  if (cf.feature_block_ratio() <= 0) {
    Range<Key> range(-1, 0);
    for (const auto& info : infos) range.setUnion(Range<Key>(info.col()));
    blocks.push_back(std::make_pair(-1, range));
  } else {
    for (const auto& info : infos) {
      CHECK(info.has_nnz_per_row());
      CHECK(info.has_id());
      float b = std::round(
          std::max((float)1.0, info.nnz_per_row() * cf.feature_block_ratio()));
      int n = std::max((int)b, 1);
      for (int i = 0; i < n; ++i) {
        auto block = Range<Key>(info.col()).evenDivide(n, i);
        if (block.empty()) continue;
        blocks.push_back(std::make_pair(info.id(), block));
      }
    }
  }
  fprintf(stderr, "features are partitioned into %lu blocks\n", blocks.size());
  return blocks;
}


} // namespace LM
} // namespace PS
