#include "graph_partition/parsa_scheduler.h"
namespace PS {
namespace GP {

void ParsaScheduler::init() {
  GraphPartition::init();
  CHECK_LT(conf_.parsa().num_partitions(), 64) << " TODO, my appologies";
}

void ParsaScheduler::run() {
  Task partitionU = newTask(Call::PARTITION_U);
  taskpool(kActiveGroup)->submitAndWait(partitionU);

  Task partitionV = newTask(Call::PARTITION_V);
  taskpool(kActiveGroup)->submitAndWait(partitionV);
}

} // namespace PS
} // namespace GP
