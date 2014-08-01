#include "risk_minimization/risk_minimization.h"
#include "base/range.h"
#include "base/matrix_io.h"
#include "util/split.h"

namespace PS {

void RiskMinimization::process(Message* msg) {
  typedef RiskMinCall Call;
  switch (getCall(*msg).cmd()) {
    case Call::EVALUATE_PROGRESS: {
      auto prog = evaluateProgress();
      string reply;
      prog.SerializeToString(&reply);
      sys_.reply(msg->sender, msg->task, reply);
      msg->replied = true;
      break;
    }
    case Call::PREPARE_DATA:
      prepareData(*msg);
      break;
    case Call::UPDATE_MODEL:
      updateModel(msg);
      break;
    case Call::SAVE_MODEL:
      saveModel(*msg);
      break;
    case Call::RECOVER:
      // FIXME
      // W_.recover();
      break;
    case Call::COMPUTE_VALIDATION_AUC: {
      AUCData data;
      computeEvaluationAUC(&data);
      string reply;
      data.SerializeToString(&reply);
      sys_.reply(msg->sender, msg->task, reply);
      msg->replied = true;
      break;
    }
    case Call::SAVE_AS_DENSE:
      saveAsDenseData(*msg);
      break;
    default:
      CHECK(false) << "unknown cmd: " << getCall(*msg).cmd();
  }
}


void RiskMinimization::mergeProgress(int iter) {
  RiskMinProgress recv;
  CHECK(recv.ParseFromString(exec_.lastRecvReply()));
  auto& p = global_progress_[iter];

  p.set_objv(p.objv() + recv.objv());
  p.set_nnz_w(p.nnz_w() + recv.nnz_w());

  if (recv.busy_time_size() > 0) p.add_busy_time(recv.busy_time(0));
  p.set_total_time(total_timer_.get());
  p.set_relative_objv(iter==0 ? 1 : global_progress_[iter-1].objv()/p.objv() - 1);
  p.set_violation(std::max(p.violation(), recv.violation()));
  p.set_nnz_active_set(p.nnz_active_set() + recv.nnz_active_set());
}

void RiskMinimization::mergeAUC(AUC* auc) {
  if (exec_.lastRecvReply().empty()) return;
  AUCData recv;
  CHECK(recv.ParseFromString(exec_.lastRecvReply()));
  auc->merge(recv);
}

void RiskMinimization::showTime(int iter) {
  if (iter == -3) {
    fprintf(stderr, "|    time (sec.)\n");
  } else if (iter == -2) {
    fprintf(stderr, "|(app:min max) total\n");
  } else if (iter == -1) {
    fprintf(stderr, "+-----------------\n");
  } else {
    auto prog = global_progress_[iter];
    double ttl_t = prog.total_time() - (
        iter > 0 ? global_progress_[iter-1].total_time() : init_sys_time_);

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


void RiskMinimization::showObjective(int iter) {
  if (iter == -3) {
    fprintf(stderr, "     |        training        ");
  } else if (iter == -2) {
    fprintf(stderr, "iter |  objective    relative ");
  } else if (iter == -1) {
    fprintf(stderr, " ----+------------------------");
  } else {
    auto prog = global_progress_[iter];
    fprintf(stderr, "%4d | %.5e  %.3e ",
            iter, prog.objv(), prog.relative_objv()); //o, prog.training_auc());
  }
}

void RiskMinimization::showNNZ(int iter) {
  if (iter == -3) {
    fprintf(stderr, "|  sparsity ");
  } else if (iter == -2) {
    fprintf(stderr, "|     |w|_0 ");
  } else if (iter == -1) {
    fprintf(stderr, "+-----------");
  } else {
    auto prog = global_progress_[iter];
    fprintf(stderr, "|%10llu ", prog.nnz_w());
  }
}

} // namespace PS
