#include "system/app.h"
#include "system/dist_monitor.h"
#include "cxxnet/proto/cxxnet.pb.h"
namespace PS {
namespace CXXNET {

class CXXNet {
 public:
  CXXNet(const Config& conf) : conf_(conf) { }
  virtual ~CXXNet() { }
 protected:
  Config conf_;
};

class CXXNetScheduler : public App, public CXXNet {
 public:
  CXXNetScheduler(const string& name, const Config& conf)
      : App(name), CXXNet(conf), monitor_(name+"_monitor", name) {
    monitor_.setDataMerger(std::bind(&CXXNetScheduler::addProgress, this, _1, _2));
  }
  virtual ~CXXNetScheduler() { }

  virtual init() { }

  virtual run() {
    // start the monitor
    monitor_.monitor(conf_.report_interval(),
                     std::bind(&CXXNetScheduler::showProgress, this));
    // ask the workers to work
    int n = sys_.yp().num_workers();
    std::vector<Task> tasks(n);
    for (int i = 0; i < n; ++i) {
      auto arg  = task[i].mutable_cxxnet();
      arg->set_cmd(Call::UPDATE_MODEL);
      // TODO split the training data to n part
      // *arg->mutable_data() = i_th_part_of_data;
    }
    port(kWorkerGroup)->submitAndWait(tasks);
  }

 protected:
  virtual void showProgress() {
    Lock l(progress_mu_);
    // TODO
  }

  virtual void addProgress(const NodeID& sender, const Progress& prog) {
    Lock l(progress_mu_);
    num_ex_processed_ += prog.num_examples_processed();
    progress_[sender] = prog;
  }
 protected:
  DistMonitor<Progress> monitor_;

  std::unordered_map<NodeID, Progress> progress_;
  std::mutex progress_mu_;
  size_t num_ex_processed_ = 0;
};

class CXXNetWorker : public App, public CXXNet {
 public:
  CXXNetWorker(const string& name, const Config& conf)
      : App(name), CXXNet(conf), reporter_(name+"_monitor", name) { }
  virtual ~CXXNetWorker() { }

  virtual init() { }

  virtual void process(const MessagePtr& msg) {
    auto arg = msg->task.cxxnet();
    if (arg.cmd() == Call::UPDATE_MODEL) {
      // start the progress reporter
      this->reporter_.reporter(
          this->schedulerID(), arg.report_interval(), [this](Progress* prog){
            Lock l(progress_mu_);
            *prog = progress_;
            progress_.Clear();
        });
      updateModel();
    }
  }

  virtual updateModel() { }
 public:
  DistMonitor<Progress> reporter_;
  Progress progress_;
  std::mutex progress_mu_;
};

class CXXNetServer : public App, public CXXNet {
 public:
  CXXNetServer(const string& name, const Config& conf)
      : App(name), CXXNet(conf) { }
  virtual ~CXXNetServer() { }

  virtual void process(const MessagePtr& msg) {
    auto sgd = msg->task.cxxnet();
    if (sgd.cmd() == Call::SAVE_MODEL) {
      saveModel();
    }
  }
  virtual init() { }

  virtual void saveModel() { }
};


} // namespace CXXNET
} // namespace PS
