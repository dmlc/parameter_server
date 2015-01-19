#include "linear_method/linear_method.h"
// #include "linear_method/ftrl.h"
#include "linear_method/async_sgd.h"
#include "linear_method/darlin.h"
#include "linear_method/model_evaluation.h"
namespace PS {
namespace LM {

App* createApp(const string& name, const Config& conf) {
  auto my_role = Postoffice::instance().myNode().role();
  App* app = nullptr;
  if (conf.has_darlin()) {
    if (my_role == Node::SCHEDULER) {
      app = new DarlinScheduler(name, conf);
    } else if (my_role == Node::WORKER) {
      app = new DarlinWorker(name, conf);
    } else if (my_role == Node::SERVER) {
      app = new DarlinServer(name, conf);
    }
  } else if (conf.has_async_sgd()) {
    typedef double Real;
    if (my_role == Node::SCHEDULER) {
      app = new AsyncSGDScheduler(name, conf);
    } else if (my_role == Node::WORKER) {
      app = new AsyncSGDWorker<Real>(name, conf);
    } else if (my_role == Node::SERVER) {
      app = new AsyncSGDServer<Real>(name, conf);
    }
  } else if (conf.has_validation_data()) {
    app =  new ModelEvaluation(name, conf);
  }
  CHECK(app) << "fail to create " << conf.ShortDebugString()
             << " at " << Postoffice::instance().myNode().ShortDebugString();
  // app->init(conf);
  return app;
}

} // namespace LM
} // namespace PS
