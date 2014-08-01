#pragma once
#include "system/app.h"
#include "base/matrix.h"
#include "base/auc.h"

namespace PS {

class RiskMinimization : public App {
 public:
  void process(Message* msg);
  void mergeProgress(int iter);
  void mergeAUC(AUC* auc);
 protected:
  virtual void prepareData(const Message& msg) = 0;
  virtual void updateModel(Message* msg) = 0;
  virtual RiskMinProgress evaluateProgress() = 0;
  virtual void saveModel(const Message& msg) = 0;
  virtual void computeEvaluationAUC(AUCData *data) = 0;
  virtual void saveAsDenseData(const Message& msg) { }

  void showTime(int iter);
  void showObjective(int iter);
  void showNNZ(int iter);

 protected:
  RiskMinCall getCall(const Message& msg) {
    CHECK_EQ(msg.task.type(), Task::CALL_CUSTOMER);
    CHECK(msg.task.has_risk());
    return msg.task.risk();
  }

  RiskMinCall* setCall(Task *task) {
    task->set_type(Task::CALL_CUSTOMER);
    return task->mutable_risk();
  }

  // progress of all iterations, only valid for the scheduler
  std::map<int, RiskMinProgress> global_progress_;
  double init_sys_time_ = 0;
};

} // namespace PS
