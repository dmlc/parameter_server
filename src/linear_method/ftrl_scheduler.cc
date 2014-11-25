#include "linear_method/ftrl_scheduler.h"
#include "linear_method/ftrl_common.h"
namespace PS {
namespace LM {

void FTRLScheduler::run() {
  // start the system
  startSystem();

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
  SArray<real> objv;
  SArray<real> auc;
  for (const auto& it : recent_progress_) {
    auto& prog = it.second;
    num_ex += prog.num_ex_trained();
    nnz_w += prog.nnz_w();
    for (int i = 0; i < prog.objv_size(); ++i) {
      objv.pushBack(prog.objv(i));
    }
    for (int i = 0; i < prog.auc_size(); ++i) {
      auc.pushBack(prog.auc(i));
    }
  }
  recent_progress_.clear();
  num_ex_processed_ += num_ex;

  printf("%10lu examples, loss %.3e +- %.3e, auc %.4f +- %.4f, |w|_0 %8llu\n",
         num_ex_processed_ , objv.mean(), objv.std(), auc.mean(), auc.std(), nnz_w);
}
} // namespace LM
} // namespace PS
