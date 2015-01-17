#include "cxxnet/cxxnet_node.h"
#include "cxxnet/ps-inl.h"
namespace PS {
namespace CXXNET {
using namespace mshadow;

void Print1DTensor(Tensor<cpu, 1, float> const &ts) {
  for (index_t i = 0; i < ts.size(0); ++i) {
    printf("%.2f ", ts[i]);
  }
  printf("\n");
}

void Print2DTensor(Tensor<cpu, 2, float> const &ts) {
  for (index_t i = 0; i < ts.size(0); ++i) {
    Print1DTensor(ts[i]);
  }
}

class PressureScheduler : public CXXNetScheduler {
 public:
  PressureScheduler(const string& name, const Config& conf)
      : CXXNetScheduler(name, conf) { }
  virtual ~PressureScheduler() { }
 protected:
  virtual void init() {
    LL << "init scheduler";
  }

  virtual void run() {
    Task task; task.mutable_sgd()->set_cmd(SGDCall::UPDATE_MODEL);
    port(kWorkerGroup)->submitAndWait(task);
  }

  virtual void showProgress() {

  }

  virtual void addProgress(const NodeID& sender, const SGDProgress& prog) {

  }
};


class PressureServer : public CXXNetServer {
 public:
  PressureServer(const string& name, const Config& conf)
      : CXXNetServer(name, conf) { }
  virtual ~PressureServer() { }
 protected:
  virtual void init() {
    ps_ = new ps::ParamServer<cpu, float>();
    ps_->SetParam("name", (name_+"_model").c_str());
    ps_->SetParam("parent_name", name_.c_str());
    ps_->Init(std::vector<int>({0}));
    LL << "init " << myNodeID();
  }

  ps::ParamServer<cpu, float>* ps_;
};


class PressureWorker : public CXXNetWorker {
 public:
  PressureWorker(const string& name, const Config& conf)
      : CXXNetWorker(name, conf) { }
  virtual ~PressureWorker() { }

  virtual void init() {
    ps_ = new ps::ParamServer<cpu, float>();
    ps_->SetParam("name", (name_+"_model").c_str());
    ps_->SetParam("parent_name", name_.c_str());
    ps_->Init(std::vector<int>({0}));
    LL << "init " << myNodeID();
  }

  virtual void updateModel() {
    LL << "update";
    TensorContainer<cpu, 2, float> ts(false);
    ts.Resize(Shape2(5,2));
    ts = 1.0f;

    TensorContainer<cpu, 2, float> res(false);
    res.Resize(Shape2(5,2));

    for (int i = 0; i < 10; ++i) {
      ps_->Push(ts, 3, 0);
      ps_->PullReq(res, 3, 0, 0, [&](Stream<cpu> *stream) {
          ts += 1.0f;
        });
    }

    ps_->PullWait(3, 0);
    Print2DTensor(res);

    LL << "done";
  }
 protected:

  ps::ParamServer<cpu, float>* ps_;
};

} // namespace CXXNET
} // namespace PS
