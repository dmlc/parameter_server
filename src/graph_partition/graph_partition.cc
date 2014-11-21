#include "graph_partition/graph_partition.h"
#include "graph_partition/parsa_scheduler.h"
#include "graph_partition/parsa_worker.h"
#include "graph_partition/parsa_server.h"

namespace PS {
namespace GP {

AppPtr GraphPartition::create(const Config& conf) {
  auto my_role = Postoffice::instance().myNode().role();
  if (conf.has_parsa()) {
    if (my_role == Node::SCHEDULER) {
      return AppPtr(new ParsaScheduler());
    } else if (my_role == Node::WORKER) {
      return AppPtr(new ParsaWorker());
    } else if (my_role == Node::SERVER) {
      return AppPtr(new ParsaServer());
    }
  }

  CHECK(false) << "unknow config" << conf.DebugString();
  return AppPtr(nullptr);
}

} // namespace GP
} // namespace PS
