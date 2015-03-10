#include "learner/workload_pool.h"
#include "data/common.h"
namespace PS {


void WorkloadPool::set(const Workload& load) {
  Lock l(mu_);
  CHECK_GT(load.replica(), 0);
  DataConfig files = searchFiles(load.data());
  VLOG(1) << "find " << files.file_size() << "files: " << files.ShortDebugString();

  // divide them
  loads_.reserve(files.file_size() * load.replica());

  int k = 0;
  for (int r = 0; r < load.replica(); ++r) {
    if (load.shuffle()) files = shuffleFiles(files);
    for (int i = 0; i < files.file_size(); ++i) {
      WorkloadInfo info;
      *info.load.mutable_data() = ithFile(files, i);
      info.load.set_id(k);
      loads_[k++] = info;
    }
  }
}


bool WorkloadPool::assign(const NodeID& node_id, Workload* load) {
  Lock l(mu_);
  for (auto& info : loads_) {
    if (!info.assigned) {
      *load = info.load;
      info.node = node_id;
      info.assigned = true;
      return true;
    }
  }
  return false;
}


void WorkloadPool::restore(const NodeID& node_id) {
  Lock l(mu_);
  for (auto& info : loads_) {
    if (info.assigned && !info.finished && info.node == node_id) {
      info.node.clear();
      info.assigned = false;
    }
  }
}


void WorkloadPool::finish(int id) {
  Lock l(mu_);
  CHECK_GE(id, 0); CHECK_LT(id, loads_.size());
  loads_[id].finished = true;
  ++ num_finished_;
}


void WorkloadPool::waitUtilDone() {
  while (true) {
    mu_.lock();
    bool done = num_finished_ >= loads_.size();
    mu_.unlock();
    if (done) {
      break;
    } else {
      usleep(1000);
    }
  }
}

} // namespace PS
