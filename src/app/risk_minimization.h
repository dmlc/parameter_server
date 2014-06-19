#pragma once
#include "app/app.h"
#include "base/matrix.h"

namespace PS {

class RiskMin : public App {
 public:
  void process(Message* msg);

 // protected:
  virtual void prepareData(const Message& msg) = 0;
  virtual void updateModel(Message* msg) = 0;
  virtual RiskMinProgress evaluateProgress() = 0;

  void mergeProgress(int iter);
  void showProgress(int iter);

 protected:
  // progress of all iterations, only valid for the scheduler
  std::map<int, RiskMinProgress> all_prog_;

  RiskMinCall getCall(const Message& msg) {
    CHECK_EQ(msg.task.type(), Task::CALL_CUSTOMER);
    CHECK(msg.task.has_risk());
    return msg.task.risk();
  }

  RiskMinCall* setCall(Task *task) {
    task->set_type(Task::CALL_CUSTOMER);
    return task->mutable_risk();
  }

 protected:
  // local training data, format:
  // label, feature_group 1, feature_group 2, ....
  // MatrixPtrList<double> training_data_;

  // only available at the scheduler
  std::vector<MatrixInfo> global_training_info_;
  // size_t global_training_size_ = 0;
  Range<Key> global_training_feature_range_ = Range<Key>(-1, 0);

};

} // namespace PS
