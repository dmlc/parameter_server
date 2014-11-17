#include "linear_method/ftrl_scheduler.h"
#include "linear_method/ftrl_common.h"
namespace PS {
namespace LM {

void FTRLScheduler::run() {
  // start the system
  Scheduler::startSystem();

  // the thread collects progress from workers and servers
  monitor_thr_ = unique_ptr<std::thread>(new std::thread([this]() {
        sleep(conf_.solver().eval_interval());
        while (true) {
          sleep(conf_.solver().eval_interval());
          showProgress();
        }
      }));
  monitor_thr_->detach();

  Task update = newTask(Call::UPDATE_MODEL);
  taskpool(kActiveGroup)->submitAndWait(update);

  Task save_model = newTask(Call::SAVE_MODEL);
  taskpool(kActiveGroup)->submitAndWait(save_model);
}

void FTRLScheduler::showProgress() {
  Lock l(progress_mu_);
  uint64 num_ex = 0, nnz_w = 0;
  real objv_worker = 0, objv_server = 0;
  for (const auto& it : recent_progress_) {
    auto& prog = it.second;
    if (prog.has_num_ex_trained()) {
      num_ex += prog.num_ex_trained();
      objv_worker += prog.objv();
    } else {
      nnz_w += prog.nnz_w();
      objv_server += prog.objv();
    }
  }
  recent_progress_.clear();

  num_ex_processed_ += num_ex;
  printf("%10lu examples, loss %.5e, penalty %.5e, |w|_0 %8llu\n",
         num_ex_processed_ , objv_worker/(real)num_ex, objv_server, nnz_w);
}
} // namespace LM
} // namespace PS
