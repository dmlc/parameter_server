#include "linear_method/linear_method.h"
#include "linear_method/ftrl.h"
namespace PS {
namespace LM {

App* createApp(const string& name, const Config& conf) {
  auto my_role = Postoffice::instance().myNode().role();
  App* app = nullptr;
  if (!conf.has_solver()) {
    if (conf.has_validation_data() && conf.has_model_input()) {
      // app =  new ModelEvaluation(name);
    }
  } else if (conf.solver().minibatch_size() <= 0) {
    // // batch solver
    // if (conf.has_darling()) {
    //   CREATE_APP(Darlin, name);
    // } else {
    //   CREATE_APP(Batch, name);
    // }
  } else {
    // online solver
    if (conf.has_ftrl()) {
      typedef double Real;
      if (my_role == Node::SCHEDULER) {
        app = new FTRLScheduler(name, conf);
      } else if (my_role == Node::WORKER) {
        app = new FTRLWorker<Real>(name, conf);
      } else if (my_role == Node::SERVER) {
        app = new FTRLServer<Real>(name, conf);
      }
    }
  }
  CHECK(app) << "fail to create " << conf.ShortDebugString()
             << " at " << Postoffice::instance().myNode().ShortDebugString();
  // app->init(conf);
  return app;
}

} // namespace LM
} // namespace PS
