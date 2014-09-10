#pragma once
#include "system/app.h"
#include "base/matrix.h"
#include "base/auc.h"
#include "proto/instance.pb.h"

namespace PS {

class RiskMinimization : public App {
 public:
  void process(const MessagePtr& msg);
  void mergeProgress(int iter);
  void mergeAUC(AUC* auc);
 protected:
  // load the data, and return the data info
  virtual InstanceInfo prepareData(const MessagePtr& msg) = 0;
  // update model
  virtual void updateModel(const MessagePtr& msg) = 0;
  // compute objective, time, ...
  virtual RiskMinProgress evaluateProgress() = 0;
  virtual void saveModel(const MessageCPtr& msg) = 0;
  virtual void computeEvaluationAUC(AUCData *data) = 0;

  // virtual void saveAsDenseData(const Message& msg) { }

  void showTime(int iter);
  void showObjective(int iter);
  void showNNZ(int iter);

  static RiskMinCall get(const MessagePtr& msg) {
    CHECK_EQ(msg->task.type(), Task::CALL_CUSTOMER);
    CHECK(msg->task.has_risk());
    return msg->task.risk();
  }

  static RiskMinCall* setCall(Task *task) {
    task->set_type(Task::CALL_CUSTOMER);
    return task->mutable_risk();
  }

  static Task newTask(RiskMinCall::Command cmd) {
    Task task; setCall(&task)->set_cmd(cmd);
    return task;
  }

  // progress of all iterations, only valid for the scheduler
  std::map<int, RiskMinProgress> global_progress_;

  Timer timer_;
};

} // namespace PS
