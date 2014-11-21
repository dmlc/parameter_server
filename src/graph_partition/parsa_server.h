#pragma once
#include "graph_partition/parsa_model.h"
#include "graph_partition/graph_partition.h"
namespace PS {
namespace GP {

class ParsaServer : public GraphPartition {
 public:
  virtual void init() {
    GraphPartition::init();
    model_ = std::shared_ptr<ParsaModel>(new ParsaModel());
    REGISTER_CUSTOMER(app_cf_.parameter_name(0), model_);
  }


 private:
  std::shared_ptr<ParsaModel> model_;
};

} // namespace GP
} // namespace PS
