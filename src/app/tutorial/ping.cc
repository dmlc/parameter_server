#include "ps.h"
namespace PS {

class HelloServer : public App {
 public:
  virtual void ProcessRequest(const MessagePtr& req) {
    LL << req->task.time() << " " << req->task.msg();
  }
};

class HelloWorker : public App {
 public:
  virtual void ProcessResponse(const MessagePtr& res) {
    LL << res->task.time();
  }

  virtual void Run() {
    WaitServersReady();
    for (int i = 0; i < 10; ++i) {
      int ts = Submit(Task(), kServerGroup);
      Wait(ts, kServerGroup);
    }
  }
};

App* App::Create(const std::string& conf) {
  if (IsWorker()) {
    return new HelloWorker();
  } else if (IsServer()) {
    return new HelloServer();
  } else {
    return new App();
  }
}

}  // namespace PS

int main(int argc, char *argv[]) {
  return PS::RunSystem(argc, argv);
}
