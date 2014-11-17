#pragma once
#include "system/app.h"
#include "parameter/kv_vector.h"

#include "linear_method/linear_method.pb.h"

#include "linear_method/loss_inl.h"
#include "linear_method/penalty_inl.h"
// #include "linear_method/learner/learner.h"
// #include "linear_method/learner/aggregate_gradient.h"

namespace PS {
namespace LM {

// linear classification/regerssion
class LinearMethod : public App {
 public:
  static AppPtr create(const Config& conf);
  virtual void init();

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
 protected:

  Config conf_;
  Timer total_timer_;
  Timer busy_timer_;

  LossPtr<double> loss_;
  PenaltyPtr<double> penalty_;
  // shared_ptr<AggGradLearner<double>> learner_;
};

} // namespace LM
} // namespace PS
