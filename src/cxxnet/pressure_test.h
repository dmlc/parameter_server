#include "cxxnet/cxxnet_node.h"
#include "mshadow-ps/ps.h"
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
    ps_ = ps::Create<cpu, float>("dist");
    ps_->SetParam("name", (name_+"_model").c_str());
    ps_->SetParam("parent_name", name_.c_str());
    ps_->Init(std::vector<int>({0}));
    LL << "init " << myNodeID();
  }

  ps::IParamServer<cpu, float>* ps_;
};


class PressureWorker : public CXXNetWorker {
 public:
  PressureWorker(const string& name, const Config& conf)
      : CXXNetWorker(name, conf) { }
  virtual ~PressureWorker() { }

  virtual void init() {
    ps_ = ps::Create<cpu, float>("dist");
    ps_->SetParam("name", (name_+"_model").c_str());
    ps_->SetParam("parent_name", name_.c_str());
    ps_->Init(std::vector<int>({0}));
    LL << "init " << myNodeID();
  }

  virtual void updateModel() {
    LL << conf_.DebugString();

    auto test = conf_.pressure_test();
    int n = test.param_size();
    std::vector<TensorContainer<cpu, 3, float>> grad(n);
    std::vector<TensorContainer<cpu, 3, float>> weight(n);
    std::vector<int> key(n);
    for (int i = 0; i < n; i++) {
      key[i] = test.param(i).key();
      auto sp = Shape3(test.ndevice(), test.param(i).height(), test.param(i).width());
      grad[i].Resize(sp);
      grad[i] = 1.0f;
      weight[i].Resize(sp);
    }

    // TODO device
    for (int rp = 0; rp < test.repeat(); ++rp) {
      for (int i = 0; i < n; ++i) {
        int d = 0;
        if (rp > 0) ps_->PullWait(key[i], d);
        ps_->Push(grad[i], key[i], d);
        ps_->PullReq(weight[i], key[i], d, 0, [&grad, i](Stream<cpu> *stream) {
            grad[i] += 1.0f;
          });
      }
    }

    for (int i = 0; i < n; ++i) {
      int d = 0;
      ps_->PullWait(key[i], d);
    //   LL << "key " << key[i];
    //   Print2DTensor(weight[i][0]);
    }
    LL << "done";
  }
 protected:

  ps::IParamServer<cpu, float>* ps_;
};

} // namespace CXXNET
} // namespace PS
