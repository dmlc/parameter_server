#include "system/app.h"
namespace PS {
// test via:
// ./local.sh 2 2 ../src/system/app_test.conf
class AppTest : public App {
 public:
  AppTest(const string& name) : App(name) {
    LL << "create app " << name_ << " at " << myNodeID();
  }
  virtual ~AppTest() {
    LL << "destroy " << name_ << " at " << myNodeID();
  }

  void init() {
    LL << "init app at " << myNodeID();
  }
  void run() {
    LL << "run app at " << myNodeID();
  }

  void process(const MessagePtr& msg) { }
};

} // namespace PS
