#include "system/app.h"
#include "parameter/kv_vector.h"
namespace PS {

class Hello : public App {
 public:
  Hello(const string& name) : App(name) { }
  virtual ~Hello() { }

  void init() {
    model_ = new KVVector<uint64, float>(name()+"_model", name());
    LL << "this is " << myNodeID() << ", a " <<
        (isWorker() ? "worker" :
         (isServer() ? "server" :
          (isScheduler() ? "scheduler" : "idle")))
              << " node with rank " << myRank()
       << ", creates model ["
       << model_->name() << "] in app [" << name() << "]";

    // initial the weight at server
    if (isServer()) {
      model_->key() = SArray<uint64>({0, 1, 2, 3, 4, 5});
      model_->value() = SArray<float>({.0, .1, .2, .3, .4, .5});
    }
  }

  void run() {
    if (isWorker()) {
      // pull
      if (myRank() == 0) {
        model_->key() = SArray<uint64>({0, 2, 4, 5});
      } else {
        model_->key() = SArray<uint64>({0, 1, 3, 4});
      }
      MessagePtr msg(new Message(kServerGroup));
      msg->key = model_->key();
      int pull_time = model_->pull(msg);

      model_->waitOutMsg(kServerGroup, pull_time);
      LL << myNodeID() << ": key: " << model_->key()
         << "; value: " << model_->value();
    }
  }

 private:
  KVVector<uint64, float> *model_;
};

App* App::create(const string& name, const string& conf) {
  return new Hello(name);
}

} // namespace PS

int main(int argc, char *argv[]) {
  PS::Postoffice::instance().start(argc, argv);

  PS::Postoffice::instance().stop();
  return 0;
}
