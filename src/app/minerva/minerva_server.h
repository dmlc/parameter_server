#include "ps.h"
#include "updater.h"
#include "shared_model.h"
#include "minerva_ps.h"

namespace PS{
  namespace minerva {

class MinervaServer : public PS::App {
public:
  MinervaServer() : App() {
    updater_ = new Updater<float>();
    shared_model_ = new PS::SharedModel<float>();
    shared_model_->setUpdater(updater_);
  }

  virtual void init() {
    LOG(ERROR) << "this is server " << myRank();
  }

  virtual void initLayer(const std::string & layerName, float * data, size_t size)
  {
    shared_model_->setLayer(layerName, data, size);
  }

  virtual ~MinervaServer() {
    delete updater_;
    delete shared_model_;
  }

private:
  Updater<float> *updater_;
  PS::SharedModel<float> *shared_model_;
};

  }
}