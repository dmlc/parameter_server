#include "graph_partition/parsa_scheduler.h"
namespace PS {
namespace GP {

void ParsaScheduler::init() {
  GraphPartition::init();
  CHECK_LT(conf_.parsa().num_partitions(), 64) << " TODO, my appologies";
}

void ParsaScheduler::run() {
  LL << "xxx";
}

} // namespace PS
} // namespace GP
