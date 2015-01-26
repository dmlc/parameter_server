#pragma once
#include "util/producer_consumer.h"
#include "learner/proto/sgd.pb.h"
#include "system/monitor.h"
#include "system/postmaster.h"
#include "system/app.h"

#include "data/stream_reader.h"
#include "util/localizer.h"
#include "parameter/frequency_filter.h"
namespace PS {

// interface for stochastic gradient descent solver
class ISGDScheduler : public App {
 public:
  ISGDScheduler(const string& name)
      : App(name), monitor_(this) { }
  virtual ~ISGDScheduler() { }

 protected:
  void saveModel() {
    Task task;
    task.mutable_sgd()->set_cmd(SGDCall::SAVE_MODEL);
    port(kServerGroup)->submitAndWait(task);
  }

  void updateModel(const DataConfig& data, int report_interval) {
    // init monitor
    using namespace std::placeholders;
    monitor_.setMerger(std::bind(&ISGDScheduler::mergeProgress, this, _1, _2));
    monitor_.setPrinter(
        report_interval, std::bind(&ISGDScheduler::showProgress, this, _1, _2));

    // ask the workers to commpute the gradients
    auto conf = Postmaster::partitionData(data, sys_.yp().num_workers());
    std::vector<Task> tasks(conf.size());
    for (int i = 0; i < conf.size(); ++i) {
      auto sgd = tasks[i].mutable_sgd();
      sgd->set_cmd(SGDCall::UPDATE_MODEL);
      *sgd->mutable_data() = conf[i];
    }
    port(kWorkerGroup)->submitAndWait(tasks);
  }

  virtual void showProgress(
      double time, std::unordered_map<NodeID, SGDProgress>* progress) {
    uint64 num_ex = 0, nnz_w = 0;
    SArray<double> objv, auc, acc;
    for (const auto& it : *progress) {
      auto& prog = it.second;
      num_ex += prog.num_examples_processed();
      nnz_w += prog.nnz();
      for (int i = 0; i < prog.objective_size(); ++i) {
        objv.pushBack(prog.objective(i));
      }
      for (int i = 0; i < prog.auc_size(); ++i) {
        auc.pushBack(prog.auc(i));
      }
      for (int i = 0; i < prog.accuracy_size(); ++i) {
        acc.pushBack(prog.accuracy(i));
      }
    }
    progress->clear();
    num_ex_processed_ += num_ex;
    fprintf(stderr, "%4d sec, %.2e examples, loss %.3e, auc %.4f, acc %.4f, |w|_0 %.2e\n",
           (int)time, (double)num_ex_processed_ , objv.sum()/(double)num_ex,
           auc.mean(), acc.mean(), (double)nnz_w);
  }

  virtual void mergeProgress(const SGDProgress& src, SGDProgress* dst) {
    auto old = *dst; *dst = src;
    // TODO also append objv
    dst->set_num_examples_processed(
        dst->num_examples_processed() + old.num_examples_processed());
  }
  MonitorMaster<SGDProgress> monitor_;
  size_t num_ex_processed_ = 0;
};

class ISGDCompNode : public App {
 public:
  ISGDCompNode(const string& name)
      : App(name), reporter_(schedulerID(), this) { }
  virtual ~ISGDCompNode() { }

 protected:
  MonitorSlaver<SGDProgress> reporter_;
};

template <typename V>
class MinibatchReader {
 public:
  MinibatchReader() { }
  ~MinibatchReader() { }

  void setReader(const DataConfig& file, int minibatch_size, int data_buf = 100) {
    reader_.init(file);
    minibatch_size_ = minibatch_size;
    data_prefetcher_.setCapacity(data_buf);
  }

  void setFilter(size_t n, int k, int freq) {
    filter_.resize(n, k);
    key_freq_ = freq;
  }

  void start() {
    data_prefetcher_.startProducer(
        [this](MatrixPtrList<V>* data, size_t* size)->bool {
          bool ret = reader_.readMatrices(minibatch_size_, data);
          for (const auto& mat : *data) {
            *size += mat->memSize();
          }
          return ret;
        });
  }

  bool read(MatrixPtr<V>& Y, MatrixPtr<V>& X, SArray<Key>& key) {
    MatrixPtrList<V> data;
    if (!data_prefetcher_.pop(&data)) return false;
    CHECK_EQ(data.size(), 2);
    Y = data[0];

    // localizer
    SArray<Key> uniq_key;
    SArray<uint8> key_cnt;
    Localizer<Key, V> localizer;
    localizer.countUniqIndex(data[1], &uniq_key, &key_cnt);

    // filter keys
    filter_.insertKeys(uniq_key, key_cnt);
    key = filter_.queryKeys(uniq_key, key_freq_);

    // to local keys
    X = localizer.remapIndex(key);
    return true;
  }
 private:
  int minibatch_size_ = 1000;
  StreamReader<V> reader_;
  FreqencyFilter<Key, uint8> filter_;
  int key_freq_ = 0;
  ProducerConsumer<MatrixPtrList<V>> data_prefetcher_;
};

} // namespace PS
