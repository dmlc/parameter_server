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
  // load global data information
  CHECK(app_cf_.has_training_data());
  auto tr_cf = searchFiles(app_cf_.training_data());
  InstanceInfo tr_info = readInstanceInfo(tr_cf);
  for (int i = 1; i < tr_info.fea_group_size(); ++i) {
    g_training_info_.push_back(readMatrixInfo<double>(tr_info, i));
  }
  g_fea_range_ = Range<Key>(
      tr_info.fea_group(0).fea_begin(), tr_info.fea_group(0).fea_end());
  g_num_training_ins_ = tr_info.num_ins();
  fprintf(stderr, "training data info: %lu examples with feature range %s\n",
          g_num_training_ins_, g_fea_range_.toString().data());

  DataConfig va_cf;
  if (app_cf_.has_validation_data()) {
    va_cf = searchFiles(app_cf_.validation_data());
    InstanceInfo va_info = readInstanceInfo(va_cf);
    for (int i = 1; i < va_info.fea_group_size(); ++i) {
      g_validation_info_.push_back(readMatrixInfo<double>(va_info, i));
    }
    g_fea_range_ = g_fea_range_.setUnion(
        Range<Key>(va_info.fea_group(0).fea_begin(),
                   va_info.fea_group(0).fea_end()));
  }

  // initialize other nodes'
  Task start;
  start.set_request(true);
  start.set_customer(name());
  start.set_type(Task::MANAGE);
  start.mutable_mng_node()->set_cmd(ManageNode::INIT);

  int s = 0;
  for (auto& it : sys_.yp().nodes()) {
    auto node = it.second;
    auto key = node.role() != Node::SERVER ? g_fea_range_ :
               g_fea_range_.evenDivide(FLAGS_num_servers, s++);
    key.to(node.mutable_key());
    *start.mutable_mng_node()->add_node() = node;
  }

  // let the scheduler connect all other nodes
  sys_.manageNode(start);

  // create the app on other nodes
  std::vector<DataConfig> split_tr_cf, split_va_cf;
  split_tr_cf = assignDataToNodes(tr_cf, FLAGS_num_workers);
  if (app_cf_.has_validation_data()) {
    split_va_cf = assignDataToNodes(va_cf, FLAGS_num_workers);
  }
  int time = 0, k = 0;
  start.mutable_mng_app()->set_cmd(ManageApp::ADD);

  for (auto& w : exec_.group(kActiveGroup)) {
    auto cf = app_cf_;
    cf.clear_training_data();
    cf.clear_validation_data();
    if (w->role() == Node::WORKER) {
      if (app_cf_.has_validation_data()) {
        *cf.mutable_validation_data() = split_va_cf[k];
      }
      *cf.mutable_training_data() = split_tr_cf[k++];
    }
    *(start.mutable_mng_app()->mutable_app_config()) = cf;
    CHECK_EQ(time, w->submit(start));
  }

  taskpool(kActiveGroup)->waitOutgoingTask(time);
  // fprintf(stderr, "system started...");
}

} // namespace LM
} // namespace PS
