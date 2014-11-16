#include "linear_method/batch_server.h"
namespace PS {
namespace LM {

void BatchServer::init(const string& name, const Config& conf) {
  conf_ = conf;
  model_ = KVBufferedVectorPtr(new KVBufferedVector<Key, double>());
  REGISTER_CUSTOMER(name, model_);
}

void BatchServer::saveModel(const DataConfig& output) {
  if (output.format() == DataConfig::TEXT) {
    CHECK(output.file_size());
    std::string file = output.file(0) + "_" + model_->myNodeID();
    std::ofstream out(file); CHECK(out.good());
    for (int grp : fea_grp_) {
      auto key = w_->key(grp);
      auto value = w_->value(grp);
      CHECK_EQ(key.size(), value.size());
      // TODO use the model_file in msg
      for (size_t i = 0; i < key.size(); ++i) {
        double v = value[i];
        if (v != 0 && !(v != v)) out << key[i] << "\t" << v << "\n";
      }
    }
    LI << model_->myNodeID() << " written the model to " << file;
  }
}


void BatchServer::preprocessData(int time, const Call& cmd) {
  for (int i = 0; i < grp_size; ++i, time += kPace) {
    if (hit_cache) continue;
    w_->waitInMsg(kWorkerGroup, time);
    w_->finish(kWorkerGroup, time+1);
  }
  for (int i = 0; i < grp_size; ++i, time += kPace) {
    w_->waitInMsg(kWorkerGroup, time);
    int chl = fea_grp_[i];
    w_->keyFilter(chl).clear();
    w_->value(chl).resize(w_->key(chl).size());
    w_->value(chl).setValue(conf_.init_w());
    w_->finish(kWorkerGroup, time+1);
  }
}
} // namespace LM
} // namespace PS
