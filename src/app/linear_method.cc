#include "app/linear_method.h"
#include "base/range.h"
#include "util/eigen3.h"
#include "base/matrix_io.h"
#include "proto/instance.pb.h"
#include "base/io.h"

namespace PS {

void LinearMethod::init() {
  w_ = KVVectorPtr(new KVVector<Key, double>());
  w_->name() = app_cf_.parameter_name(0);
  sys_.yp().add(std::static_pointer_cast<Customer>(w_));

  bool has_learner = app_cf_.has_learner();
  if (has_learner) {
    learner_ = std::static_pointer_cast<AggGradLearner<double>>(
        LearnerFactory<double>::create(app_cf_.learner()));
  }

  if (app_cf_.has_loss()) {
    loss_ = LossFactory<double>::create(app_cf_.loss());
    if (has_learner) learner_->setLoss(loss_);
  }

  if (app_cf_.has_penalty()) {
    penalty_ = PenaltyFactory<double>::create(app_cf_.penalty());
    if (has_learner) learner_->setPenalty(penalty_);
  }
}

void LinearMethod::startSystem() {
  // load global data information
  int num_servers = FLAGS_num_servers;
  int num_workers = FLAGS_num_workers;
  InstanceInfo training_info, validation_info;
  std::vector<DataConfig> divided_training, divided_validation;

  CHECK(app_cf_.has_training_data());
  divided_training = assignDataToNodes(
      app_cf_.training_data(), num_workers, &training_info);

  for (int i = 0; i < training_info.individual_groups_size(); ++i) {
    global_training_info_.push_back(
        readMatrixInfo<double>(training_info.individual_groups(i)));
  }
  global_feature_range_ = Range<Key>(
      training_info.all_group().feature_begin(),
      training_info.all_group().feature_end());

  global_training_example_size_ =
      global_training_info_[0].row().end() -
      global_training_info_[0].row().begin();

  if (app_cf_.has_validation_data()) {
    divided_validation = assignDataToNodes(
        app_cf_.validation_data(), num_workers, &validation_info);
    for (int i = 0; i < validation_info.individual_groups_size(); ++i) {
      global_validation_info_.push_back(
          readMatrixInfo<double>(validation_info.individual_groups(i)));
    }
    global_feature_range_ = global_feature_range_.setUnion(
        Range<Key>(validation_info.all_group().feature_begin(),
                   validation_info.all_group().feature_end()));
  }
  fprintf(stderr, "training data info: %lu examples with feature range %s\n",
          global_training_example_size_, global_feature_range_.toString().data());

  // initialize other nodes'
  Task start;
  start.set_request(true);
  start.set_customer(name());
  start.set_type(Task::MANAGE);
  start.mutable_mng_node()->set_cmd(ManageNode::INIT);

  App::requestNodes();
  int s = 0;
  for (auto& it : nodes_) {
    auto& node = it.second;
    auto key = node.role() != Node::SERVER ? global_feature_range_ :
               global_feature_range_.evenDivide(num_servers, s++);
    key.to(node.mutable_key());
    *start.mutable_mng_node()->add_nodes() = node;
  }

  // let the scheduler connect all other nodes
  sys_.manage_node(start);

  // create the app on other nodes
  int time = 0, k = 0;
  start.mutable_mng_app()->set_cmd(ManageApp::ADD);
  for (auto& w : exec_.group(kActiveGroup)) {
    auto cf = app_cf_;
    cf.clear_training_data();
    cf.clear_validation_data();
    if (w->role() == Node::CLIENT) {
      if (app_cf_.has_validation_data())
        *cf.mutable_validation_data() = divided_validation[k];
      *cf.mutable_training_data() = divided_training[k++];
    }
    *(start.mutable_mng_app()->mutable_app_config()) = cf;
    CHECK_EQ(time, w->submit(start));
  }
  taskpool(kActiveGroup)->waitOutgoingTask(time);
  fprintf(stderr, "system started...");

  // load data, build key mapping
  Task prepare;
  prepare.set_type(Task::CALL_CUSTOMER);
  prepare.mutable_risk()->set_cmd(RiskMinCall::PREPARE_DATA);
  taskpool(kActiveGroup)->submitAndWait(prepare);
  fprintf(stderr, "loaded data... in %.3f sec\n", total_timer_.get());
}

void LinearMethod::saveModel(const Message& msg) {
  // didn't use msg here. in future, one may pass the model_file by msg

  if (!exec_.isServer()) return;
  if (!app_cf_.has_model_output()) return;

  auto output = app_cf_.model_output();
  // if (output.files_size() != 1) {
  //   LL << "you should use only a single file: " << output.DebugString();
  //   return;
  // }

  CHECK_EQ(w_->key().size(), w_->value().size());

  if (output.format() == DataConfig::TEXT) {
    std::string file = w_->name() + "_" + exec_.myNode().id();
    std::ofstream out(file);
    CHECK(out.good());

    for (size_t i = 0; i < w_->key().size(); ++i) {
      if (w_->value()[i] != 0)
        out << w_->key()[i] << "\t" << w_->value()[i] << "\n";
    }
  } else {
    LL << "didn't implement yet";
  }
}

} // namespace PS
