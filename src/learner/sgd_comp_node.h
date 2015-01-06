#pragma once
#include "learner/sgd_node.h"
namespace PS {

class SGDCompNode : public SGDNode {
 public:
  virtual void evaluateProgress(SGDProgress* prog) = 0;
  void startReporter(int interval) {
    reporter_thr_ = unique_ptr<std::thread>(new std::thread([this, interval]() {
          while (true) {
            sleep(interval);
            SGDProgress prog; evaluateProgress(&prog);
            string str; CHECK(prog.SerializeToString(&str));
            Task report = newTask(SGDCall::REPORT_PROGRESS);
            report.set_msg(str);
            auto sch = taskpool(schedulerID());
            if (!sch) continue;
            sch->submit(report);
          }
        }));
  }

  // no need for a worker/server
  virtual void run() { }
 protected:
  unique_ptr<std::thread> reporter_thr_;
  SGDProgress progress_;
  std::mutex progress_mu_;
};

} // namespace PS
