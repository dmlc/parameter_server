#pragma once
#include "system/app.h"
namespace PS {
namespace GP {
class GraphPartition : public App {
 public:
  static AppPtr create(const Config& conf);

  static Call get(const MessageCPtr& msg) {
    CHECK_EQ(msg->task.type(), Task::CALL_CUSTOMER);
    CHECK(msg->task.has_graph_partition());
    return msg->task.graph_partition();
  }
  static Call* set(Task *task) {
    task->set_type(Task::CALL_CUSTOMER);
    return task->mutable_graph_partition();
  }
  static Task newTask(Call::Command cmd) {
    Task task; set(&task)->set_cmd(cmd);
    return task;
  }


  virtual void init() {
    conf_ = app_cf_.graph_partition();
  }
  virtual void run() { }
  // virtual void process(const MessagePtr& msg) { }
 protected:
  Config conf_;
};

} // namespace GP
} // namespace PS
