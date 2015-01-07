#pragma once
#include "system/app.h"
#include "factorization_machine/fm.pb.h"
namespace PS {
namespace FM {

typedef double Real;

class FactorizationMachine {
 public:
  static AppPtr create(const Config& conf);

  // static Call get(const MessageCPtr& msg) {
  //   CHECK_EQ(msg->task.type(), Task::CALL_CUSTOMER);
  //   CHECK(msg->task.has_factorization_machine());
  //   return msg->task.factorization_machine();
  // }
  // static Call* set(Task *task) {
  //   task->set_type(Task::CALL_CUSTOMER);
  //   return task->mutable_factorization_machine();
  // }
  // static Task newTask(Call::Command cmd) {
  //   Task task; set(&task)->set_cmd(cmd);
  //   return task;
  // }

};
} // namespace FM
} // namespace PS
