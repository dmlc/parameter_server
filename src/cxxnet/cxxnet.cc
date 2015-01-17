// put all includes here to accelerate (re-)compiling
#include "cxxnet/cxxnet.h"
#include "cxxnet/cxxnet_node.h"
#include "cxxnet/pressure_test.h"
namespace PS {
namespace CXXNET {

App* createApp(const string& name, const CXXNetConfig& conf) {
  auto my_role = Postoffice::instance().myNode().role();

  if (conf.has_pressure_test()) {
    if (my_role == Node::SCHEDULER) {
      app = new PressureScheduler(name, conf);
    } else if (my_role == Node::WORKER) {
      app = new PressureWorker(name, conf);
    } else if (my_role == Node::SERVER) {
      app = new PressureServer(name, conf);
    }
  } else {
    if (my_role == Node::SCHEDULER) {
      app = new CXXNetScheduler(name, conf);
    } else if (my_role == Node::WORKER) {
      app = new CXXNetWorker(name, conf);
    } else if (my_role == Node::SERVER) {
      app = new CXXNetServer(name, conf);
    }
  }
}

}

} // namespace CXXNET
} // namespace PS
