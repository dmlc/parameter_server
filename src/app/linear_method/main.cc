#include "ps.h"
#include "app/linear_method/async_sgd.h"
#include "app/linear_method/darlin.h"
#include "app/linear_method/model_evaluation.h"

namespace PS {
App* App::Create(const string& conf_str) {
  using namespace LM;
  // parse config
  Config conf;
  CHECK(google::protobuf::TextFormat::ParseFromString(conf_str, &conf))
      << " failed to parse conf: " << conf.ShortDebugString();

  // create app
  auto my_role = MyNode().role();
  App* app = nullptr;
  if (conf.has_darlin()) {
    if (my_role == Node::SCHEDULER) {
      app = new DarlinScheduler(conf);
    } else if (my_role == Node::WORKER) {
      app = new DarlinWorker(conf);
    } else if (my_role == Node::SERVER) {
      app = new DarlinServer(conf);
    }
  } else if (conf.has_async_sgd()) {
    typedef float Real;
    if (my_role == Node::SCHEDULER) {
      app = new AsyncSGDScheduler(conf);
    } else if (my_role == Node::WORKER) {
      app = new AsyncSGDWorker<Real>(conf);
    } else if (my_role == Node::SERVER) {
      app = new AsyncSGDServer<Real>(conf);
    }
  } else if (conf.has_validation_data()) {
    app = new ModelEvaluation(conf);
  }
  CHECK(app) << "fail to create " << conf.ShortDebugString()
             << " at " << MyNode().ShortDebugString();
  return app;
}
} // namespace PS

int main(int argc, char *argv[]) {
  PS::RunSystem(argc, argv);
  return 0;
}
