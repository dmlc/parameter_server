#include "factorization_machine/fm_scheduler.h"
namespace PS {
namespace FM {

void FMScheduler::run() {
  // start the system
  // startSystem();

  // the thread collects progress from workers and servers
  monitor_thr_ = unique_ptr<std::thread>(new std::thread([this]() {
        // sleep(conf_.solver().eval_interval());
        // while (true) {
        //   sleep(conf_.solver().eval_interval());
        //   showProgress();
        // }
      }));
  monitor_thr_->detach();

  Task update = newTask(Call::UPDATE_MODEL);
  port(kActiveGroup)->submitAndWait(update);

  Task save_model = newTask(Call::SAVE_MODEL);
  port(kActiveGroup)->submitAndWait(save_model);
}
} // namespace FM
} // namespace PS
