#pragma once
#include "system/app.h"
#include "learner/sgd_node.h"
namespace PS {

class SGDScheduler : public App, public SGDNode {
 public:
  SGDScheduler(const string& name) : App(name) { }
  virtual ~SGDScheduler() { }

  virtual void init() {}
  virtual void run() {
    Postmaster sch(static_cast<Customer*>(this));

    // divide the key space and start all server nodes
    auto nodes = sch.partitionKey(exec_.servers(), Range<Key>::all());

    // divide data and start all worker nodes

    // wait finished
    startSystem();

    // save the model
    startMonitor(1);

    Task update = newTask(SGDCall::UPDATE_MODEL);
    taskpool(kActiveGroup)->submitAndWait(update);

    Task save_model = newTask(SGDCall::SAVE_MODEL);
    taskpool(kActiveGroup)->submitAndWait(save_model);
  }

  virtual void process(const MessagePtr& msg) {

  }
 protected:
  void addProgress(const NodeID& sender, const string& proto) {
    SGDProgress prog;
    CHECK(prog.ParseFromString(proto));
    Lock l(progress_mu_);
    recent_progress_.push_back(std::make_pair(sender, prog));
  }

  void startMonitor(int interval) {
    monitor_thr_ = unique_ptr<std::thread>(
        new std::thread([this, interval]() {
            sleep(interval);
            while (true) { sleep(interval); showProgress(); }
        }));
    monitor_thr_->detach();
  }

  virtual void showProgress() {
    // TODO

  }

 protected:
  unique_ptr<std::thread> monitor_thr_;
  std::vector<std::pair<NodeID, SGDProgress>> recent_progress_;
  std::mutex progress_mu_;
};

} // namespace PS
