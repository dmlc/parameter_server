#pragma once
#include "learner/sgd.pb.h"
#include "system/app.h"
namespace PS {

class SGDNode : public App {
 public:
  static SGDCall get(const MessageCPtr& msg) {
    CHECK_EQ(msg->task.type(), Task::CALL_CUSTOMER);
    CHECK(msg->task.has_sgd());
    return msg->task.sgd();
  }
  static SGDCall* set(Task *task) {
    task->set_type(Task::CALL_CUSTOMER);
    return task->mutable_sgd();
  }
  static Task newTask(SGDCall::Command cmd) {
    Task task; set(&task)->set_cmd(cmd);
    return task;
  }
};
} // namespace PS
