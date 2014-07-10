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
  int num_worker = FLAGS_num_workers;

  std::vector<Range<Key>> server_range;
  std::vector<DataConfig> worker_training_;

  auto data = searchFiles(app_cf_.training());
  CHECK_GT(data.files_size(), 0);
  // LL << "training data " << data.DebugString();

  if (data.format() == DataConfig::BIN) {
    // format: Y, feature group 0, feature group 1, ...
    // assume those data are shared by all workers, the first one is the label,
    // and the second one is the training data

    // while each of the rest present one feature group.
    // FIXME how to store the
    // feature group info
    MatrixInfo info;
    for (int i = 1; i < data.files_size(); ++i) {
      ReadFileToProtoOrDie(data.files(i)+".info", &info);
      global_training_info_.push_back(info);
      global_training_feature_range_  =
          global_training_feature_range_.setUnion(Range<Key>(info.col()));
    }
    SizeR global_data_range = SizeR(info.row());
    for (int i = 0; i < num_worker; ++i) {
      global_data_range.evenDivide(num_worker, i).to(data.mutable_range());
      worker_training_.push_back(data);
    }
  } else if (data.format() == DataConfig::PROTO) {
    // assume multiple recordio files, each worker get several of them
    InstanceInfo info;
    for (int i = 0; i < data.files_size(); ++i) {
      InstanceInfo f;
      ReadFileToProtoOrDie(data.files(i)+".info", &f);
      info = i == 0 ? f : mergeInstanceInfo(info, f);
    }
    for (int i = 0; i < info.individual_groups_size(); ++i) {
      global_training_info_.push_back(
          readMatrixInfo<double>(info.individual_groups(i)));
    }
    global_training_feature_range_ =
        Range<Key>(info.all_group().feature_begin(), info.all_group().feature_end());
    for (int i = 0; i < num_worker; ++i) {
      DataConfig dc; dc.set_format(DataConfig::PROTO);
      auto os = Range<int>(0, data.files_size()).evenDivide(num_worker, i);
      for (int j = os.begin(); j < os.end(); ++j)
        dc.add_files(data.files(j));
      worker_training_.push_back(dc);
    }
  }
  global_training_example_size_ += global_training_info_[0].row().end() -
                                   global_training_info_[0].row().begin();

  fprintf(stderr, "training data info: %lu examples with feature range %s\n",
          global_training_example_size_, global_training_feature_range_.toString().data());

  // evenly divide the keys range for server nodes
  for (int i = 0; i < num_servers; ++i) {
    server_range.push_back(
        global_training_feature_range_.evenDivide(num_servers, i));
  }

  App::requestNodes();
  int s = 0;
  for (auto& it : nodes_) {
    auto& d = it.second;
    if (d.role() == Node::SERVER)
      server_range[s++].to(d.mutable_key());
    else
      global_training_feature_range_.to(d.mutable_key());
  }
  CHECK_EQ(s, FLAGS_num_servers);

  Task start;
  // must set the following two here, though often will set automatically by calling submit()
  start.set_request(true);
  start.set_customer(name());
  start.set_type(Task::MANAGE);
  start.mutable_mng_node()->set_cmd(ManageNode::INIT);
  for (auto& it : nodes_)
    *start.mutable_mng_node()->add_nodes() = it.second;

  // LL << start.DebugString();
  // let the scheduler connect all other nodes
  sys_.manage_node(start);

  // create the app on other nodes
  int time = 0, k = 0;
  start.mutable_mng_app()->set_cmd(ManageApp::ADD);
  for (auto& w : exec_.group(kActiveGroup)) {
    auto cf = app_cf_; cf.clear_training();
    if (w->role() == Node::CLIENT)
      *cf.mutable_training() = worker_training_[k++];
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
