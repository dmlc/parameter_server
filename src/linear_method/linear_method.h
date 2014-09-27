#pragma once
#include "system/app.h"
#include "parameter/kv_vector.h"
#include "proto/linear_method.pb.h"

#include "linear_method/loss_inl.h"
#include "linear_method/penalty_inl.h"
#include "base/auc.h"
// #include "linear_method/learner/learner.h"
// #include "linear_method/learner/aggregate_gradient.h"

namespace PS {
namespace LM {

// linear classification/regerssion
class LinearMethod : public App {
 public:
  virtual void init();

  void process(const MessagePtr& msg);
  void mergeProgress(int iter);
  void mergeAUC(AUC* auc);

 protected:
  void startSystem();

  // load the data, and return 1 if hit cache, 0 if normal
  virtual int loadData(const MessageCPtr& msg, ExampleInfo* info) = 0;
  virtual void preprocessData(const MessageCPtr& msg) = 0;
  virtual void updateModel(const MessagePtr& msg) = 0;
  virtual Progress evaluateProgress() = 0;
  virtual void saveModel(const MessageCPtr& msg) = 0;
  virtual void computeEvaluationAUC(AUCData *data) = 0;

  void showTime(int iter);
  void showObjective(int iter);
  void showNNZ(int iter);

  static Call get(const MessageCPtr& msg) {
    CHECK_EQ(msg->task.type(), Task::CALL_CUSTOMER);
    CHECK(msg->task.has_linear_method());
    return msg->task.linear_method();
  }
  static Call* set(Task *task) {
    task->set_type(Task::CALL_CUSTOMER);
    return task->mutable_linear_method();
  }
  static Task newTask(Call::Command cmd) {
    Task task; set(&task)->set_cmd(cmd);
    return task;
  }

  // progress of all iterations, only valid for the scheduler
  std::map<int, Progress> g_progress_;

  Config conf_;
  Timer total_timer_;
  Timer busy_timer_;

  LossPtr<double> loss_;
  PenaltyPtr<double> penalty_;
  // shared_ptr<AggGradLearner<double>> learner_;

};

} // namespace LM
} // namespace PS
  // virtual void saveAsDenseData(const Message& msg) { }
