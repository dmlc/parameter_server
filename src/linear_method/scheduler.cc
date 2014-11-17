#include "linear_method/scheduler.h"
#include "data/common.h"
namespace PS {
namespace LM {

void Scheduler::process(const MessagePtr& msg) {
  switch (get(msg).cmd()) {
    // the computation nodes report progress to the scheduler
    case Call::REPORT_PROGRESS: {
      Progress prog; CHECK(prog.ParseFromString(msg->task.msg()));
      Lock l(progress_mu_);
      recent_progress_[msg->sender] = prog;
      break;
    }
    default:
      CHECK(false) << "unknown cmd: " << get(msg).cmd();
  }
}

void Scheduler::startSystem() {
  Task start;
  start.set_request(true);
  start.set_customer(name());
  start.set_type(Task::MANAGE);
  start.mutable_mng_node()->set_cmd(ManageNode::INIT);

  Range<Key> key_range = Range<Key>::all();
  int s = 0;
  for (auto& it : sys_.yp().nodes()) {
    auto node = it.second;
    auto key = node.role() != Node::SERVER ? key_range :
               key_range.evenDivide(FLAGS_num_servers, s++);
    key.to(node.mutable_key());
    *start.mutable_mng_node()->add_node() = node;
  }
  // let the scheduler connect all other nodes
  sys_.manageNode(start);

  // find all training (validation) files, and assign them to workers
  CHECK(conf_.has_training_data());
  std::vector<DataConfig> tr_parts, va_parts;
  auto tr_cf = searchFiles(conf_.training_data());
  LI << "Found " << tr_cf.file_size() << " training files";
  tr_parts = divideFiles(tr_cf, FLAGS_num_workers);
  int n = 0; for (const auto& p : tr_parts) n += p.file_size();
  LI << "Assigned " << n << " files to " << FLAGS_num_workers << " workers";

  if (conf_.has_validation_data()) {
    auto va_cf = searchFiles(conf_.validation_data());
    va_parts = divideFiles(va_cf, FLAGS_num_workers);
    LI << "Found " << va_cf.file_size() << " validation files";
  }

  // create the app on all other machines
  int time = 1;  // time 0 is used for
  int k = 0;
  start.mutable_mng_app()->set_cmd(ManageApp::ADD);
  start.set_time(time);
  for (auto& w : exec_.group(kActiveGroup)) {
    auto cf = app_cf_;
    auto lm = cf.mutable_linear_method();
    lm->clear_training_data();
    lm->clear_validation_data();
    if (w->role() == Node::WORKER) {
      if (conf_.has_validation_data()) {
        *lm->mutable_validation_data() = va_parts[k];
      }
      *lm->mutable_training_data() = tr_parts[k++];
    }
    *start.mutable_mng_app()->mutable_app_config() = cf;
    CHECK_EQ(time, w->submit(start));
  }
  taskpool(kActiveGroup)->waitOutgoingTask(time);
}

void Scheduler::mergeProgress(int iter) {
  Progress recv;
  CHECK(recv.ParseFromString(exec_.lastRecvReply()));
  auto& p = g_progress_[iter];
  if (p.objv_size() == 0) p.add_objv(0);
  for (int i = 0; i < recv.objv_size(); ++i) {
    p.set_objv(0, p.objv(0) + recv.objv(i));
  }
  p.set_nnz_w(p.nnz_w() + recv.nnz_w());

  if (recv.busy_time_size() > 0) p.add_busy_time(recv.busy_time(0));
  p.set_total_time(total_timer_.stop());
  total_timer_.start();
  p.set_relative_objv(iter==0 ? 1 : g_progress_[iter-1].objv(0)/p.objv(0) - 1);
  p.set_violation(std::max(p.violation(), recv.violation()));
  p.set_nnz_active_set(p.nnz_active_set() + recv.nnz_active_set());
}

void Scheduler::mergeAUC(AUC* auc) {
  if (exec_.lastRecvReply().empty()) return;
  AUCData recv;
  CHECK(recv.ParseFromString(exec_.lastRecvReply()));
  auc->merge(recv);
}

void Scheduler::showTime(int iter) {
  if (iter == -3) {
    fprintf(stderr, "|    time (sec.)\n");
  } else if (iter == -2) {
    fprintf(stderr, "|(app:min max) total\n");
  } else if (iter == -1) {
    fprintf(stderr, "+-----------------\n");
  } else {
    auto prog = g_progress_[iter];
    double ttl_t = prog.total_time() - (
        iter > 0 ? g_progress_[iter-1].total_time() : 0);

    int n = prog.busy_time_size();
    Eigen::ArrayXd busy_t(n);
    for (int i = 0; i < n; ++i) {
      busy_t[i] = prog.busy_time(i);
      // if (iter > 0) busy_t[i] -= global_progress_[iter-1].busy_time(i);
    }
    // double mean = busy_t.sum() / n;
    // double var = (busy_t - mean).matrix().norm() / std::sqrt((double)n);
    fprintf(stderr, "|%6.1f%6.1f%6.1f\n", busy_t.minCoeff(), busy_t.maxCoeff(), ttl_t);
  }
}


void Scheduler::showObjective(int iter) {
  if (iter == -3) {
    fprintf(stderr, "     |        training        ");
  } else if (iter == -2) {
    fprintf(stderr, "iter |  objective    relative ");
  } else if (iter == -1) {
    fprintf(stderr, " ----+------------------------");
  } else {
    auto prog = g_progress_[iter];
    double objv = prog.objv_size() ? prog.objv(0) : 0;
    fprintf(stderr, "%4d | %.5e  %.3e ",
            iter, objv, prog.relative_objv());
  }
}

void Scheduler::showNNZ(int iter) {
  if (iter == -3) {
    fprintf(stderr, "|  sparsity ");
  } else if (iter == -2) {
    fprintf(stderr, "|     |w|_0 ");
  } else if (iter == -1) {
    fprintf(stderr, "+-----------");
  } else {
    auto prog = g_progress_[iter];
    fprintf(stderr, "|%10lu ", (size_t)prog.nnz_w());
  }
}
} // namespace LM
} // namespace PS
