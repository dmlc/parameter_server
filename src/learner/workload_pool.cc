#include "learner/workload_pool.h"
#include "data/common.h"
namespace PS {

void WorkloadPool::set(const Workload& load) {
  VLOG(1) << "init workload " << load.ShortDebugString();
  Lock l(mu_);
  CHECK_GT(load.replica(), 0);
  DataConfig files = searchFiles(load.data());
  VLOG(1) << "find " << files.file_size() << " files: " << files.ShortDebugString();

  loads_.resize(files.file_size() * load.replica());
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
  CHECK_EQ(k, loads_.size());
}


bool WorkloadPool::assign(const NodeID& node_id, Workload* load) {
  Lock l(mu_);
  for (auto& info : loads_) {
    if (!info.assigned) {
      *load = info.load;
      info.node = node_id;
      info.assigned = true;
      VLOG(1) << "assign to [" << node_id << "] " << load->ShortDebugString();
      return true;
    }
  }
  return false;
}


void WorkloadPool::restore(const NodeID& node_id) {
  Lock l(mu_);
  for (auto& info : loads_) {
    if (info.assigned && !info.finished && info.node == node_id) {
      // info.node = "";
      info.assigned = false;
      VLOG(1) << "restore workload " << info.load.id() << " from " << node_id;
    }
  }
}


void WorkloadPool::finish(int id) {
  Lock l(mu_);
  CHECK_GE(id, 0); CHECK_LT(id, loads_.size());
  loads_[id].finished = true;
  ++ num_finished_;
  VLOG(1) << "workload " << id << " is finished";
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
  VLOG(1) << "all workloads are done";
}

} // namespace PS
