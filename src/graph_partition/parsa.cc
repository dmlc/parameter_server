#include "graph_partition/parsa.h"
#include "graph_partition/parsa_worker.h"
#include "graph_partition/parsa_server.h"
namespace PS {

void Parsa::init() {
  conf_ = app_cf_.parsa();
  CHECK_LT(conf_.num_partitions(), 64) << " TODO, my appologies";
  num_partitions_ = conf_.num_partitions();
  // worker_ = std::unique_ptr<ParsaWorker>(new ParsaWorker(conf_));
  worker_ = new ParsaWorker(conf_);
}

void Parsa::run() {
  worker_->partition();
}

} // namespace PS
