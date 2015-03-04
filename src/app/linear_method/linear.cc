#include "ps.h"
#include "app/linear_method/linear.h"
#include "app/linear_method/async_sgd.h"
#include "app/linear_method/darlin.h"
#include "app/linear_method/model_evaluation.h"
namespace PS {
namespace LM {

App* createApp(const string& name, const Config& conf) {
  // auto my_role = Postoffice::instance().myNode().role();
  auto my_role = MyNode().role();
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
    typedef float Real;
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
             << " at " << MyNode().ShortDebugString();
  return app;
}

} // namespace LM
} // namespace PS
