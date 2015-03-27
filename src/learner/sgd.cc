#include "learner/sgd.h"
namespace PS {

ISGDScheduler::~ISGDScheduler() {
  // core dump when delete workload_pool_;
}

void ISGDScheduler::Run() {
  // init monitor
  using namespace std::placeholders;
  monitor_.set_merger(std::bind(&ISGDScheduler::MergeProgress, this, _1, _2));
  monitor_.set_printer(1, std::bind(&ISGDScheduler::ShowProgress, this, _1, _2));

  // wait all jobs are finished
  sys_.manager().AddNodeFailureHandler([this](const NodeID& id) {
      CHECK_NOTNULL(workload_pool_)->restore(id);
    });
  CHECK_NOTNULL(workload_pool_)->waitUtilDone();

  // save model
  Task task;
  task.mutable_sgd()->set_cmd(SGDCall::SAVE_MODEL);
  int ts = Submit(task, kServerGroup);
  Wait(ts);
}

void ISGDScheduler::ProcessResponse(Message* response) {
  const auto& sgd = response->task.sgd();
  if (sgd.cmd() == SGDCall::UPDATE_MODEL) {
    for (int i = 0; i < sgd.load().finished_size(); ++i) {
      workload_pool_->finish(sgd.load().finished(i));
    }
    SendWorkload(response->sender);
  }
}

void ISGDScheduler::ProcessRequest(Message* request) {
  if (request->task.sgd().cmd() == SGDCall::REQUEST_WORKLOAD) {
    SendWorkload(request->sender);
  }
}

void ISGDScheduler::SendWorkload(const NodeID& recver) {
  Task task;
  task.mutable_sgd()->set_cmd(SGDCall::UPDATE_MODEL);
  if (workload_pool_->assign(recver, task.mutable_sgd()->mutable_load())) {
    Submit(task, recver);
  }
}

void ISGDScheduler::ShowProgress(
    double time, std::unordered_map<NodeID, SGDProgress>* progress) {
  uint64 num_ex = 0, nnz_w = 0;
  SArray<double> objv, auc, acc;
  double weight_sum = 0, delta_sum = 1e-20;
  for (const auto& it : *progress) {
    auto& prog = it.second;
    num_ex += prog.num_examples_processed();
    nnz_w += prog.nnz();
    for (int i = 0; i < prog.objective_size(); ++i) {
      objv.push_back(prog.objective(i));
    }
    for (int i = 0; i < prog.auc_size(); ++i) {
      auc.push_back(prog.auc(i));
    }
    for (int i = 0; i < prog.accuracy_size(); ++i) {
      acc.push_back(prog.accuracy(i));
    }
    weight_sum += prog.weight_sum();
    delta_sum += prog.delta_sum();
  }
  progress->clear();
  num_ex_processed_ += num_ex;
  if (show_prog_head_) {
    NOTICE(" sec  examples    loss      auc   accuracy   |w|_0  updt ratio");
    show_prog_head_ = false;
  }
  NOTICE("%4d  %.2e  %.3e  %.4f  %.4f  %.2e  %.2e",
         (int)time,
         (double)num_ex_processed_ ,
         objv.Sum()/(double)num_ex,
         auc.Mean(),
         acc.Mean(),
         (double)nnz_w,
         sqrt(delta_sum) / sqrt(weight_sum));
}


void ISGDScheduler::MergeProgress(const SGDProgress& src, SGDProgress* dst) {
  auto old = *dst; *dst = src;
  // TODO also append objv
  dst->set_num_examples_processed(
      dst->num_examples_processed() + old.num_examples_processed());
}

} // namespace PS
