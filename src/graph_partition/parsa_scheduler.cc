#include "graph_partition/parsa_scheduler.h"
#include "graph_partition/parsa_common.h"
#include "system/postmaster.h"
namespace PS {
namespace GP {

void ParsaScheduler::init() {
  GraphPartition::init();
  CHECK_LE(conf_.parsa().num_partitions(), kMaxNumPartitions) << " TODO, my appologies";
}

void ParsaScheduler::run() {
  Postmaster sch(static_cast<Customer*>(this));

  // divide key
  auto nodes = sch.partitionKey(sch.nodes(), Range<Key>::all());

  // divide data
  auto data = sch.partitionData(conf_.input_graph(), FLAGS_num_workers);
  std::vector<AppConfig> apps(nodes.size(), app_cf_);
  for (int i = 0; i < nodes.size(); ++i) {
    auto gp = apps[i].mutable_graph_partition();
    gp->clear_input_graph();
    if (nodes[i].role() == Node::WORKER) {
      *gp->mutable_input_graph() = data.back();
      data.pop_back();
    }
  }

  // start the system
  sch.createApp(nodes, apps);

  auto parsa = conf_.parsa();
  if (parsa.stage0_blocks()) {
    Task partitionU = newTask(Call::PARTITION_U_STAGE_0);
    CHECK(taskpool("W0"));
    taskpool("W0")->submitAndWait(partitionU);
  }

  if (parsa.stage1_block_size()) {
    Task partitionU = newTask(Call::PARTITION_U_STAGE_1);
    taskpool(kActiveGroup)->submitAndWait(partitionU);
  }

  Task partitionV = newTask(Call::PARTITION_V);
  taskpool(kActiveGroup)->submitAndWait(partitionV);
}

} // namespace PS
} // namespace GP
