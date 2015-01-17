#pragma once
#include "system/app.h"
#include "system/dist_monitor.h"
#include "cxxnet/proto/cxxnet.pb.h"
#include "learner/proto/sgd.pb.h"
#include "learner/sgd.h"
namespace PS {
namespace CXXNET {

class CXXNet {
 public:
  CXXNet(const Config& conf) : conf_(conf) { }
  virtual ~CXXNet() { }
 protected:
  Config conf_;
};

class CXXNetScheduler : public ISGDScheduler, public CXXNet {
 public:
  CXXNetScheduler(const string& name, const Config& conf)
      : ISGDScheduler(name), CXXNet(conf) { }
  virtual ~CXXNetScheduler() { }

  virtual void init() {
    // TODO
  }

  virtual void run() {
    // TODO
  }

 protected:
  virtual void showProgress() {
    // TODO
  }

};

class CXXNetWorker : public ISGDCompNode, public CXXNet {
 public:
  CXXNetWorker(const string& name, const Config& conf)
      : ISGDCompNode(name), CXXNet(conf) { }
  virtual ~CXXNetWorker() { }

  virtual void init() {
    // TODO
  }

  virtual void process(const MessagePtr& msg) {
    auto arg = msg->task.sgd();
    if (arg.cmd() == SGDCall::UPDATE_MODEL) {
      // start the progress reporter
      startReporter(arg.report_interval());
      // update
      updateModel();
    }
  }

  virtual void evaluate(SGDProgress* prog) {
    // TODO
  }
  virtual void updateModel() {
    // TODO
  }
 public:
  // SGDProgress progress_;
  // std::mutex progress_mu_;
};

class CXXNetServer : public App, public CXXNet {
 public:
  CXXNetServer(const string& name, const Config& conf)
      : App(name), CXXNet(conf) { }
  virtual ~CXXNetServer() { }

  virtual void process(const MessagePtr& msg) {
    auto arg = msg->task.sgd();
    if (arg.cmd() == SGDCall::SAVE_MODEL) {
      saveModel();
    }
  }
  virtual void init() {
    // TODO
  }

  virtual void saveModel() {
    // TODO
  }
};


} // namespace CXXNET
} // namespace PS
