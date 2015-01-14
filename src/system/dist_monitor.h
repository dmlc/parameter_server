#pragma once
#include "system/customer.h"
namespace PS {

// Data should be a protobuf class
template<class Data>
class DistMonitor : Customer {
 public:
  DistMonitor(const string& my_name, const string& parent_name)
      : Customer(my_name, parent_name) { }

  virtual ~DistMonitor() {
    done_ = true;
    if (monitor_thr_) monitor_thr_->join();
    if (reporter_thr_) reporter_thr_->join();
  }

  typedef std::function<void (const NodeID& sender, const Data&)> Merger;
  void setDataMerger(Merger merger) {
    merger_ = merger;
  }

  void process(const MessagePtr& msg) {
    Data data;
    CHECK(data.ParseFromString(msg->task.msg()));
    if (merger_) merger_(msg->sender, data);
  }


  template<class Printer>
  void monitor(int interval, Printer printer) {
    monitor_thr_ = unique_ptr<std::thread>(
        new std::thread([this, interval, printer]() {
            sleep(interval);
            while (true) {
              sleep(interval);
              if (done_) break;
              printer();
            }
        }));
  }

  template<class Eval>
  void reporter(NodeID monitor, int interval, Eval eval) {
    reporter_thr_ = unique_ptr<std::thread>(
        new std::thread([this, monitor, interval, eval]() {
            while (true) {
              sleep(interval);
              if (done_) break;
              Data data; eval(&data);
              string str; CHECK(data.SerializeToString(&str));
              Task report;
              report.set_type(Task::CALL_CUSTOMER);
              report.set_msg(str);
              auto mon = port(monitor);
              if (mon) mon->submit(report);
          }
        }));
  }

 private:
  bool done_ = false;
  unique_ptr<std::thread> monitor_thr_, reporter_thr_;
  Merger merger_;
};

} // namespace PS
