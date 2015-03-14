#include "ps.h"
namespace PS {

class HelloServer : public App {
 public:
  virtual void ProcessRequest(const MessagePtr& req) {
    LL << MyNodeID() <<  ": request " << req->task.time() << " from " << req->sender;
  }
};

class HelloWorker : public App {
 public:
  virtual void ProcessResponse(const MessagePtr& res) {
    LL << MyNodeID() << ": response " << res->task.time() << " from " << res->sender;
  }

  virtual void Run() {
    WaitServersReady();

    int ts = Submit(Task(), kServerGroup);
    Wait(ts, kServerGroup);

    ts = Submit(Task(), kServerGroup);
    Wait(ts, kServerGroup);

    auto req = NewMessage();
    req->recver = kServerGroup;
    req->fin_handle = [this]() {
      LL << MyNodeID() << ": request " << LastResponse()->task.time() << " is finished";
    };
    Wait(Submit(req), req->recver);

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
