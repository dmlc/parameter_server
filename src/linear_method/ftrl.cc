#include "linear_method/ftrl.h"
#include "linear_method/ftrl_server.h"
#include "linear_method/ftrl_worker.h"
#include "data/common.h"
namespace PS {
namespace LM {

void FTRL::init() {
  OnlineSolver::init();
  if (IamServer()) {
    server_ = new FTRLServer();
    server_->init(conf_);
    server_->name() = app_cf_.parameter_name(0);
    sys_.yp().add(CustomerPtr((Customer*)server_));
  } else if (IamWorker()) {
    worker_ = new FTRLWorker();
    worker_->init(app_cf_.parameter_name(0), conf_);
  }
}

void FTRL::run() {
  // start the system
  LinearMethod::startSystem();

  // the thread collects progress from workers and servers
  prog_thr_ = unique_ptr<std::thread>(new std::thread([this]() {
        sleep(conf_.solver().eval_interval());
        while (true) {
          sleep(conf_.solver().eval_interval());
          showProgress();
        }
      }));
  prog_thr_->detach();

  Task update = newTask(Call::UPDATE_MODEL);
  taskpool(kActiveGroup)->submitAndWait(update);

  Task save_model = newTask(Call::SAVE_MODEL);
  taskpool(kActiveGroup)->submitAndWait(save_model);
}

void FTRL::updateModel(const MessagePtr& msg) {
  // the thread reports progress to the scheduler
  prog_thr_ = unique_ptr<std::thread>(new std::thread([this]() {
        while (true) {
          sleep(conf_.solver().eval_interval());
          Progress prog;
          if (IamWorker()) {
            worker_->evaluateProgress(&prog);
          } else if (IamServer()) {
            server_->evaluateProgress(&prog);
          }

          auto report = newTask(Call::REPORT_PROGRESS);
          string str; CHECK(prog.SerializeToString(&str));
          report.set_msg(str);
          taskpool(schedulerID())->submit(report);
        }
      }));
  prog_thr_->detach();

  if (IamWorker()) {
    worker_->computeGradient();
  }
}

void FTRL::saveModel(const MessageCPtr& msg) {
  if (!IamServer()) return;
  if (!conf_.has_model_output()) return;
  auto out = ithFile(conf_.model_output(), 0, "_" + myNodeID());
  server_->writeToFile(out);
  LI << myNodeID() << " writes model to " << out.file(0);
}

void FTRL::showProgress() {
  Lock l(progress_mu_);
  real objv_worker = 0, objv_server = 0;
  uint64 num_ex = 0, nnz_w = 0;
  for (const auto& it : recent_progress_) {
    auto prog = it.second;
    if (prog.has_num_ex_trained()) {
      num_ex += prog.num_ex_trained();
      objv_worker += prog.objv();
    } else {
      nnz_w += prog.nnz_w();
      objv_server += prog.objv();
    }
  }
  num_ex_processed_ += num_ex;
  printf("%10lu examples, loss %.5e, penalty %.5e, |w|_0 %8llu\n",
         num_ex_processed_ , objv_worker/(real)num_ex, objv_server, nnz_w);
}

} // namespace LM
} // namespace PS
