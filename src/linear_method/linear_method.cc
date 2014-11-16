#include "linear_method/linear_method.h"
#include "base/range.h"
#include "util/eigen3.h"
#include "base/matrix_io_inl.h"
#include "proto/instance.pb.h"
#include "base/io.h"
#include "linear_method/ftrl.h"
// #include "linear_method/darling.h"
// #include "linear_method/batch_solver.h"
#include "linear_method/model_evaluation.h"
namespace PS {
namespace LM {

  if (IamWorker()) {
    worker_ = std::shared_ptr<BatchWorker>(new BatchWorker());
    worker_->init(app_cf_.parameter_name(0), conf_, this);
  } else if (IamServer()) {
    server_ = std::shared_ptr<BatchServer>(new BatchServer());
    server_->init(app_cf_.parameter_name(0), conf_);
  }
AppPtr LinearMethod::create(const Config& conf) {
  if (!conf.has_solver()) {
    if (conf.has_validation_data() && conf.has_model_input()) {
      return AppPtr(new ModelEvaluation());
    }
  } else if (conf.solver().minibatch_size() <= 0) {
    // // batch solver
    // if (conf.has_darling()) {
    //   return AppPtr(new Darling());
    // } else {
    //   return AppPtr(new BatchSolver());
    // }
  } else {
    // online solver
    if (conf.has_ftrl()) {
      return AppPtr(new FTRL());
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

void LinearMethod::process(const MessagePtr& msg) {
  switch (get(msg).cmd()) {
    case Call::EVALUATE_PROGRESS: {
      Progress prog; evaluateProgress(&prog);
      sys_.replyProtocalMessage(msg, prog);
      break;
    }
    case Call::REPORT_PROGRESS: {
      Progress prog; CHECK(prog.ParseFromString(msg->task.msg()));
      Lock l(progress_mu_);
      recent_progress_[msg->sender] = prog;
    }
    case Call::LOAD_DATA: {
      DataInfo info;
      info.set_hit_cache(loadData(msg, info.mutable_example_info()));
      sys_.replyProtocalMessage(msg, info);
      break;
    }
    case Call::PREPROCESS_DATA:
      preprocessData(msg);
      break;
    case Call::UPDATE_MODEL:
      updateModel(msg);
      break;
    case Call::SAVE_MODEL:
      saveModel(msg);
      break;
    case Call::RECOVER:
      // FIXME
      // W_.recover();
      break;
    case Call::COMPUTE_VALIDATION_AUC: {
      AUCData data;
      computeEvaluationAUC(&data);
      sys_.replyProtocalMessage(msg, data);
      break;
    }
    default:
      CHECK(false) << "unknown cmd: " << get(msg).cmd();
  }
}




} // namespace LM
} // namespace PS
