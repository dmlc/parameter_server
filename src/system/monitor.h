#pragma once
#include "ps.h"
namespace PS {

template <typename Progress>
class MonitorMaster : public Customer {
 public:
  MonitorMaster(int id = NextCustomerID()) : Customer(id) {}

  typedef std::unordered_map<NodeID, Progress> AllProgress;
  typedef std::function<void(double time, AllProgress*)> Printer;
  void setPrinter(double time_interval, Printer printer) {
    timer_.start();
    printer_ = printer;
    interval_ = time_interval;
  }

  typedef std::function<void(const Progress& src, Progress* dst)> Merger;
  void setMerger(Merger merger) {
    merger_ = merger;
  }

  void process(const MessagePtr& msg) {
    NodeID sender = msg->sender;
    Progress prog;
    CHECK(prog.ParseFromString(msg->task.msg()));
    if (merger_) {
      merger_(prog, &progress_[sender]);
    } else {
      progress_[sender] = prog;
    }

    double time = timer_.stop();
    if (time > interval_ && printer_) {
      total_time_ += time;
      printer_(total_time_, &progress_);
      timer_.restart();
    } else {
      timer_.start();
    }
  }
 private:
  AllProgress progress_;
  double interval_;
  Timer timer_;
  double total_time_ = 0;
  Merger merger_;
  Printer printer_;
};

template <typename Progress>
class MonitorSlaver : public Customer {
 public:
  MonitorSlaver(const NodeID& master, int id = NextCustomerID())
      : Customer(id), master_(master) { }
  virtual ~MonitorSlaver() { }

  void report(const Progress& prog) {
    string str; CHECK(prog.SerializeToString(&str));
    Task report; report.set_msg(str);
    auto master = port(master_);
    if (master) master->submit(report);
  }
 protected:
  NodeID master_;
};

} // namespace PS
