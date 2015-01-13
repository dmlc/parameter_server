#pragma once
#include "linear_method/proto/lm.pb.h"
#include "linear_method/loss.h"
#include "linear_method/penalty.h"
#include "system/app.h"
namespace PS {
namespace LM {

App* createApp(const string& name, const Config& conf);

// linear classification/regerssion
template<typename V>
class LinearMethod {
 public:
  LinearMethod(const Config& conf) {
    conf_ = conf;
    if (conf_.has_loss()) {
      loss_ = createLoss<V>(conf_.loss());
    }
    if (conf_.has_penalty()) {
      penalty_ = createPenalty<V>(conf_.penalty());
    }
  }
  virtual ~LinearMethod() { }

  // static Call get(const MessageCPtr& msg) {
  //   CHECK_EQ(msg->task.type(), Task::CALL_CUSTOMER);
  //   CHECK(msg->task.has_linear_method());
  //   return msg->task.linear_method();
  // }
  // static Call* set(Task *task) {
  //   task->set_type(Task::CALL_CUSTOMER);
  //   return task->mutable_linear_method();
  // }
  // static Task newTask(Call::Command cmd) {
  //   Task task; set(&task)->set_cmd(cmd);
  //   return task;
  // }
 protected:
  Config conf_;
  Timer total_timer_;
  Timer busy_timer_;

  LossPtr<V> loss_;
  PenaltyPtr<V> penalty_;
};

} // namespace LM
} // namespace PS
