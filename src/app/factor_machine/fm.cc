#include "factorization_machine/fm.h"
#include "factorization_machine/fm_worker.h"
#include "factorization_machine/fm_server.h"
#include "factorization_machine/fm_scheduler.h"
#include "system/app.h"
namespace PS {
namespace FM {

AppPtr FactorizationMachine::create(const Config& conf) {
  auto my_role = Postoffice::instance().myNode().role();
  if (my_role == Node::SCHEDULER) {
    return AppPtr(new FMScheduler());
  } else if (my_role == Node::WORKER) {
    return AppPtr(new FMWorker());
  } else if (my_role == Node::SERVER) {
    return AppPtr(new FMServer());
  }
  return AppPtr(nullptr);
}

} // namespace FM
} // namespace PS
