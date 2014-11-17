#include "linear_method/batch_server.h"
namespace PS {
namespace LM {

void BatchServer::init() {
  CompNode::init();
  model_ = KVBufferedVectorPtr<Key, double>(new KVBufferedVector<Key, double>());
  REGISTER_CUSTOMER(app_cf_.parameter_name(0), model_);
}

void BatchServer::saveModel(const DataConfig& output) {
  if (output.format() == DataConfig::TEXT) {
    CHECK(output.file_size());
    std::string file = output.file(0) + "_" + myNodeID();
    std::ofstream out(file); CHECK(out.good());
    for (int grp : fea_grp_) {
      auto key = model_->key(grp);
      auto value = model_->value(grp);
      CHECK_EQ(key.size(), value.size());
      // TODO use the model_file in msg
      for (size_t i = 0; i < key.size(); ++i) {
        double v = value[i];
        if (v != 0 && !(v != v)) out << key[i] << "\t" << v << "\n";
      }
    }
    LI << myNodeID() << " written the model to " << file;
  }
}

void BatchServer::preprocessData(const MessagePtr& msg) {
  int time = msg->task.time() * k_time_ratio_;
  auto cmd = get(msg);
  int grp_size = cmd.fea_grp_size();
  fea_grp_.clear();
  for (int i = 0; i < grp_size; ++i) {
    fea_grp_.push_back(cmd.fea_grp(i));
  }
  bool hit_cache = cmd.hit_cache();
  // filter tail keys
  for (int i = 0; i < grp_size; ++i, time += k_time_ratio_) {
    if (hit_cache) continue;
    model_->waitInMsg(kWorkerGroup, time);
    model_->finish(kWorkerGroup, time+1);
  }
  for (int i = 0; i < grp_size; ++i, time += k_time_ratio_) {
    // wait untill received all keys from workers
    model_->waitInMsg(kWorkerGroup, time);
    // initialize the weight
    int chl = fea_grp_[i];
    model_->clearTailFilter(chl);
    model_->value(chl).resize(model_->key(chl).size());
    model_->value(chl).setValue(conf_.init_w());
    model_->finish(kWorkerGroup, time+1);
  }
}

} // namespace LM
} // namespace PS
