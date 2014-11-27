#pragma once
#include "linear_method/computation_node.h"
namespace PS {
namespace LM {

class ProgressReporter {
 public:
  void start(CompNode* node, int interval) {
    node_ = node;
    thr_ = unique_ptr<std::thread>(new std::thread([this, interval]() {
          while (true) {
            sleep(interval);
            Progress prog;
            node_->evaluateProgress(&prog);
            auto report = LinearMethod::newTask(Call::REPORT_PROGRESS);
            string str; CHECK(prog.SerializeToString(&str));
            report.set_msg(str);
            auto sch = node_->taskpool(node_->schedulerID());
            if (!sch) break;
            sch->submit(report);
          }
        }));
    thr_->detach();
  }
 private:
  CompNode* node_;
  unique_ptr<std::thread> thr_;
};
} // namespace LM
} // namespace PS
