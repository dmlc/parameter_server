#include "learner/bcd.h"
namespace PS {

void BCDScheduler::SaveModel(const DataConfig& data) {
  Task task;
  task.mutable_bcd()->set_cmd(BCDCall::SAVE_MODEL);
  *task.mutable_bcd()->mutable_data() = data;
  Wait(Submit(task, kServerGroup));
}

string BCDScheduler::ShowTime(int iter) {
  char buf[500];
  if (iter == -3) {
    snprintf(buf, 500, "|    time (sec.)\n");
  } else if (iter == -2) {
    snprintf(buf, 500, "|(app:min max) total\n");
  } else if (iter == -1) {
    snprintf(buf, 500, "+-----------------\n");
  } else {
    auto prog = g_progress_[iter];
    double ttl_t = prog.total_time() - (
        iter > 0 ? g_progress_[iter-1].total_time() : 0);
    int n = prog.busy_time_size();
    Eigen::ArrayXd busy_t(n);
    for (int i = 0; i < n; ++i) {
      busy_t[i] = prog.busy_time(i);
    }
    snprintf(buf, 500, "|%6.1f%6.1f%6.1f\n", busy_t.minCoeff(), busy_t.maxCoeff(), ttl_t);
  }
  return string(buf);
}

string BCDScheduler::ShowObjective(int iter) {
  char buf[500];
  if (iter == -3) {
    snprintf(buf, 500, "     |        training        |  sparsity ");
  } else if (iter == -2) {
    snprintf(buf, 500, "iter |  objective    relative |     |w|_0 ");
  } else if (iter == -1) {
    snprintf(buf, 500, " ----+------------------------+-----------");
  } else {
    auto prog = g_progress_[iter];
    snprintf(buf, 500, "%4d | %.5e  %.3e |%10lu ",
             iter, prog.objective(), prog.relative_obj(), (size_t)prog.nnz_w());
  }
  return string(buf);
}

}  // namespace PS
