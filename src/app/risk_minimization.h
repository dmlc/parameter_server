#pragma once
#include "app/app.h"
#include "base/matrix.h"
#include "base/auc.h"

namespace PS {

class RiskMinimization : public App {
 public:
  void process(Message* msg);
  void mergeProgress(int iter);

 protected:
  virtual void prepareData(const Message& msg) = 0;
  virtual void updateModel(Message* msg) = 0;
  virtual RiskMinProgress evaluateProgress() = 0;
  virtual void saveModel(const Message& msg) = 0;

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

  // only available at the scheduler
  std::vector<MatrixInfo> global_training_info_;
  size_t global_training_example_size_ = 0;

  Range<Key> global_training_feature_range_ = Range<Key>(-1, 0);

  AUC training_auc_;
};

  // local training data, format:
  // label, feature_group 1, feature_group 2, ....
  // MatrixPtrList<double> training_data_;
} // namespace PS
