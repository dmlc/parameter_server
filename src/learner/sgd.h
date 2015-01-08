#pragma once
#include "learner/sgd.pb.h"
namespace PS {

static SGDCall getSGDCall(const MessageCPtr& msg) {
  CHECK_EQ(msg->task.type(), Task::CALL_CUSTOMER);
  CHECK(msg->task.has_sgd());
  return msg->task.sgd();
}

static SGDCall* setSGDCall(Task *task) {
  task->set_type(Task::CALL_CUSTOMER);
  return task->mutable_sgd();
}

static Task newSGDTask(SGDCall::Command cmd) {
  Task task; set(&task)->set_cmd(cmd);
  return task;
}

};

} // namespace PS
