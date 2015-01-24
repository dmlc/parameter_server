#include <iostream>
#include "ps.h"
#include "parameter/kv_vector.h"

namespace PS {
class HelloServer : public App {
 public:
  HelloServer() : App() { }
  virtual ~HelloServer() { }

  void init() {
    LL << myNodeID() << ", this is server " << myRank();

    // initial the weight at server
    model_ = new KVVector<uint64, float>("w");
    model_->key() = {0, 1, 2, 3, 4, 5};
    model_->value() = {.0, .1, .2, .3, .4, .5};
  }

 private:
  KVVector<uint64, float> *model_;
};

App* CreateServerNode(const std::string& conf) {
  return new HelloServer();
}
} // namespace PS

int WorkerNodeMain(int argc, char *argv[]) {
  using namespace PS;
  LOG(ERROR) << MyNodeID() <<  ": this is worker " << MyRank();

  KVVector<uint64, float> model("w");

  if (MyRank() == 0) {
    model.key() = {0, 2, 4, 5};
  } else {
    model.key() = {0, 1, 3, 4};
  }
  MessagePtr msg(new Message(kServerGroup));
  msg->key = model.key();
  int pull_time = model.pull(msg);

  model.waitOutMsg(kServerGroup, pull_time);
  LOG(ERROR) << MyNodeID() << ": key: " << model.key()
             << "; value: " << model.value();
  return 0;
}
