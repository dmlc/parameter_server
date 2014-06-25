#include "app/risk_minimization.h"
#include "base/range.h"
#include "base/matrix_io.h"
#include "util/split.h"

namespace PS {

void RiskMin::process(Message* msg) {
  busy_timer_.start();
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
    case Call::RECOVER:
      // W_.recover();
      // FIXME
      break;
    default:
      CHECK(false) << "unknown cmd: " << getCall(*msg).cmd();
  }
  busy_timer_.stop();
}


void RiskMin::mergeProgress(int iter) {
  RiskMinProgress recv;
  CHECK(recv.ParseFromString(exec_.lastRecvReply()));
  auto& p = all_prog_[iter];

  p.set_objv(p.objv() + recv.objv());
  p.set_nnz_w(p.nnz_w() + recv.nnz_w());

  if (recv.busy_time_size() > 0) p.add_busy_time(recv.busy_time(0));
  p.set_total_time(total_timer_.get());
  p.set_relative_objv(iter==0 ? 1 : all_prog_[iter-1].objv()/p.objv() - 1);
}


void RiskMin::showProgress(int iter) {
  auto prog = all_prog_[iter];
  // LL << prog.DebugString();

  double ttl_t = prog.total_time();
  if (iter > 0) ttl_t -= all_prog_[iter-1].total_time();

  int n = prog.busy_time_size();
  Eigen::ArrayXd busy_t(n);
  for (int i = 0; i < n; ++i) {
    busy_t[i] = prog.busy_time(i);
    if (iter > 0) busy_t[i] -= all_prog_[iter-1].busy_time(i);
  }
  double mean = busy_t.sum() / n;
  double var = (busy_t - mean).matrix().norm() / std::sqrt((double)n);

  if (iter == 0) {
    fprintf(stderr, " iter | objective    relative_obj  |w|_0 | time: app total\n");
    fprintf(stderr, " ----+----------------------------------+---------------\n");
  }
  fprintf(stderr, "%4d | %.5e  %.3e  %8lld | %4.2f+-%2.2f %4.2f\n",
          iter, prog.objv(), prog.relative_objv(), prog.nnz_w(), mean, var, ttl_t);
}

} // namespace PS

// void RiskMin::readLocalData() {
//   CHECK(exec_.client());
//   training_data_ = readMatrices<double>(app_cf_.training());
// }

// void RiskMin::readGlobalDataInfo() {
//   auto data = app_cf_.training();
//   if (data.format() == DataConfig::BIN) {
//     // format: Y, feature group 0, feature group 1, ...
//     for (int i = 1; i < data.files_size(); ++i) {
//       MatrixInfo info;
//       ReadFileToProtoOrDie(data.files(i)+".info", &info);
//       global_training_info_.push_back(info);

//       auto size = SizeR(info.row()).size();
//       if (global_training_size_ == 0)
//         global_training_size_ = size;
//       else
//         CHECK_EQ(global_training_size_, size);

//       global_training_feature_range_  =
//           global_training_feature_range_.setUnion(SizeR(info.col()));
//     }
//   } else {
//     // TODO
//   }
// }
