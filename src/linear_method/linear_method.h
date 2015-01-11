#pragma once
#include "linear_method/linear_method.pb.h"
#include "linear_method/loss.h"
#include "linear_method/penalty.h"
namespace PS {
namespace LM {

// linear classification/regerssion
template<typename V>
class LinearMethod {
 public:
  LinearMethod(const Config& conf) {
    conf_ = conf;
    if (conf_.has_loss()) {
      loss_ = createLoss<V>(conf_.loss());
    }
    if (conf_.has_penalty()) {
      penalty_ = createPenalty<V>(conf_.penalty());
    }
  }
  virtual ~LinearMethod() { }

  // static Call get(const MessageCPtr& msg) {
  //   CHECK_EQ(msg->task.type(), Task::CALL_CUSTOMER);
  //   CHECK(msg->task.has_linear_method());
  //   return msg->task.linear_method();
  // }
  // static Call* set(Task *task) {
  //   task->set_type(Task::CALL_CUSTOMER);
  //   return task->mutable_linear_method();
  // }
  // static Task newTask(Call::Command cmd) {
  //   Task task; set(&task)->set_cmd(cmd);
  //   return task;
  // }
 protected:
  Config conf_;
  Timer total_timer_;
  Timer busy_timer_;

  LossPtr<V> loss_;
  PenaltyPtr<V> penalty_;
};

} // namespace LM
} // namespace PS


#include "linear_method/ftrl.h"
namespace PS {
namespace LM {

static App* createApp(const string& name, const Config& conf) {
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
