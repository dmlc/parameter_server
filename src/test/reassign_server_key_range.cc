#include "gtest/gtest.h"
#include "system/postoffice.h"
#include "system/postmaster.h"
#include "system/app.h"

// build: make build/reassign_server_key_range
// test: script/local.sh build/reassign_server_key_range 3 3
namespace PS {
class Root : public App {
 public:
  Root() : App() { }
  virtual ~Root() { }

  void init() {
    // repartition the key range
    Range<Key> range(0, 100);
    auto nodes = sys_.yp().nodes();
    nodes = Postmaster::partitionServerKeyRange(nodes, range);

    Task task;
    task.set_type(Task::MANAGE);
    task.mutable_mng_node()->set_cmd(ManageNode::UPDATE);
    for (const auto& n : nodes) {
      *task.mutable_mng_node()->add_node() = n;
    }
    port(kLiveGroup)->submitAndWait(task);
  }
};

class Slave : public App {
 public:
  Slave() : App() { }
  virtual ~Slave() { }

  void run() {
    LL << exec_.myNode().id() << " key range: "
       << exec_.myNode().key().ShortDebugString();
  }
};

App* App::create(const string& name, const string& conf) {
  auto my_role = Postoffice::instance().myNode().role();
  if (my_role == Node::SCHEDULER) {
    return new Root();
  } else {
    return new Slave();
  }
}

} // namespace PS

int main(int argc, char *argv[]) {
  auto& sys = PS::Postoffice::instance();
  sys.start(&argc, &argv);

  sys.Stop();
  return 0;
}
