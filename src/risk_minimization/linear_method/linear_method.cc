#include "risk_minimization/linear_method/linear_method.h"
#include "risk_minimization/learner/learner_factory.h"
#include "base/range.h"
#include "util/eigen3.h"
#include "base/matrix_io_inl.h"
#include "proto/instance.pb.h"
#include "base/io.h"

namespace PS {
namespace LM {

void LinearMethod::init() {
  bool has_learner = app_cf_.has_learner();
  if (has_learner) {
    learner_ = std::static_pointer_cast<AggGradLearner<double>>(
        LearnerFactory<double>::create(app_cf_.learner()));
  }
  if (app_cf_.has_loss()) {
    loss_ = Loss<double>::create(app_cf_.loss());
    if (has_learner) learner_->setLoss(loss_);
  }
  if (app_cf_.has_penalty()) {
    penalty_ = Penalty<double>::create(app_cf_.penalty());
    if (has_learner) learner_->setPenalty(penalty_);
  }
}

void LinearMethod::startSystem() {
  // all available nodes
  Task start;
  start.set_request(true);
  start.set_customer(name());
  start.set_type(Task::MANAGE);
  start.mutable_mng_node()->set_cmd(ManageNode::INIT);

  Range<Key> key_range = Range<Key>::all();
  int s = 0;
  for (auto& it : sys_.yp().nodes()) {
    auto node = it.second;
    auto key = node.role() != Node::SERVER ? key_range :
               key_range.evenDivide(FLAGS_num_servers, s++);
    key.to(node.mutable_key());
    *start.mutable_mng_node()->add_node() = node;
  }
  // let the scheduler connect all other nodes
  sys_.manageNode(start);

  // find all training (validation) files, and assign them to workers
  CHECK(app_cf_.has_training_data());
  std::vector<DataConfig> tr_parts, va_parts;
  auto tr_cf = searchFiles(app_cf_.training_data());
  tr_parts = divideFiles(tr_cf, FLAGS_num_workers);
  if (app_cf_.has_validation_data()) {
    auto va_cf = searchFiles(app_cf_.validation_data());
    va_parts = divideFiles(va_cf, FLAGS_num_workers);
  }

  // create the app on all other machines
  int time = 1;  // time 0 is used for
  int k = 0;
  start.mutable_mng_app()->set_cmd(ManageApp::ADD);
  start.set_time(time);
  for (auto& w : exec_.group(kActiveGroup)) {
    auto cf = app_cf_;
    cf.clear_training_data();
    cf.clear_validation_data();
    if (w->role() == Node::WORKER) {
      if (app_cf_.has_validation_data()) {
        *cf.mutable_validation_data() = va_parts[k];
      }
      *cf.mutable_training_data() = tr_parts[k++];
    }
    *(start.mutable_mng_app()->mutable_app_config()) = cf;
    CHECK_EQ(time, w->submit(start));
  }
  taskpool(kActiveGroup)->waitOutgoingTask(time);
}

} // namespace LM
} // namespace PS
