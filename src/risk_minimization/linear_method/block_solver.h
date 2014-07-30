#pragma once
#include "risk_minimization/linear_method/linear_method.h"
namespace PS {
namespace LM {

class BlockSolver : public LinearMethod {
 public:
  typedef std::vector<std::pair<int, Range<Key>>> FeatureBlocks;
  static FeatureBlocks partitionFeatures(
      const AppConfig& app_cf, const std::vector<MatrixInfo>& infos);
};

} // namespace LM
} // namespace PS
