#pragma once
#include "parameter/kv_buffered_vector.h"

namespace PS {
namespace LM {

class BatchServer {
 public:
  void init(const Config& conf, const string& name);

  void saveModel(const DataConfig& output);
 protected:
  KVBufferedVectorPtr<Key, double> model_;
  Config conf_;
};

void BatchServer::init(const Config& conf, const string& name) {
  conf_ = conf;
  model_ = KVBufferedVectorPtr(new KVBufferedVector<Key, double>());
  model_->name() = name;
  Postoffice::instance().yp().add(std::static_pointer_cast<Customer>(model_));
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


} // namespace LM
} // namespace PS
