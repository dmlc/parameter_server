#include "linear_method/linear_method.h"
#include "base/range.h"
#include "util/eigen3.h"
#include "base/matrix_io_inl.h"
#include "proto/instance.pb.h"
#include "base/io.h"
#include "linear_method/darlin_worker.h"
#include "linear_method/darlin_server.h"
#include "linear_method/darlin_scheduler.h"
#include "linear_method/batch_worker.h"
#include "linear_method/batch_server.h"
#include "linear_method/batch_scheduler.h"
#include "linear_method/model_evaluation.h"
#include "linear_method/ftrl_scheduler.h"
#include "linear_method/ftrl_worker.h"
#include "linear_method/ftrl_server.h"
namespace PS {
namespace LM {

AppPtr LinearMethod::create(const Config& conf) {
  auto my_role = Postoffice::instance().myNode().role();
  if (!conf.has_solver()) {
    if (conf.has_validation_data() && conf.has_model_input()) {
      return AppPtr(new ModelEvaluation());
    }
  } else if (conf.solver().minibatch_size() <= 0) {
    // batch solver
    if (conf.has_darling()) {
      // darlin
      if (my_role == Node::SCHEDULER) {
        return AppPtr(new DarlinScheduler());
      } else if (my_role == Node::WORKER) {
        return AppPtr(new DarlinWorker());
      } else if (my_role == Node::SERVER) {
        return AppPtr(new DarlinServer());
      }
    } else {
      // general batch solver
      if (my_role == Node::SCHEDULER) {
        return AppPtr(new BatchScheduler());
      } else if (my_role == Node::WORKER) {
        return AppPtr(new BatchWorker());
      } else if (my_role == Node::SERVER) {
        return AppPtr(new BatchServer());
      }
    }
  } else {
    // online solver
    if (conf.has_ftrl()) {
      if (my_role == Node::SCHEDULER) {
        return AppPtr(new FTRLScheduler());
      } else if (my_role == Node::WORKER) {
        return AppPtr(new FTRLWorker());
      } else if (my_role == Node::SERVER) {
        return AppPtr(new FTRLServer());
      }
    }
  }
  return AppPtr(nullptr);
}

void LinearMethod::init() {
  CHECK(app_cf_.has_linear_method());
  conf_ = app_cf_.linear_method();

  if (conf_.has_loss()) {
    loss_ = Loss<double>::create(conf_.loss());
  }

  if (conf_.has_penalty()) {
    penalty_ = Penalty<double>::create(conf_.penalty());
  }

  // bool has_learner = app_cf_.has_learner();
  // if (has_learner) {
  //   learner_ = std::static_pointer_cast<AggGradLearner<double>>(
  //       LearnerFactory<double>::create(app_cf_.learner()));
  // }
  // if (has_learner) learner_->setLoss(loss_);
  // if (has_learner) learner_->setPenalty(penalty_);
}




} // namespace LM
} // namespace PS
