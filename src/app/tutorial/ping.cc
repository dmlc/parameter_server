#include "ps.h"
namespace PS {

class PingServer : public App {
 public:
  // PingServer() { }

  virtual void ProcessRequest(const MessagePtr& req) {
    LL << req->task.time() << " " << req->task.msg();
  }
};

class PingWorker : public App {
 public:
  virtual void ProcessResponse(const MessagePtr& res) {
    LL << res->task.time();
  }

  virtual void Run() {
    for (int i = 0; i < 10; ++i) {
      int ts = Submit(Task(), kServerGroup);
      Wait(ts, kServerGroup);
    }
  }
};

App* App::Create(const std::string& conf) {
  if (IsWorker()) {
    return new PingWorker();
  } else if (IsServer()) {
    return new PingServer();
  } else {
    return new App();
  }
}

}  // namespace PS


int main(int argc, char *argv[]) {
  PS::Postoffice::instance().run(&argc, &argv);
  PS::Postoffice::instance().stop();
  return 0;
}
