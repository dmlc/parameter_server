#pragma once
#include "ps.h"
#include "util/producer_consumer.h"
#include "learner/proto/sgd.pb.h"
#include "system/monitor.h"
#include "system/assigner.h"
#include "data/stream_reader.h"
#include "util/localizer.h"
#include "parameter/frequency_filter.h"
#include "learner/workload_pool.h"
namespace PS {

// interface for stochastic gradient descent solver

// the base class of a scheduler node
class ISGDScheduler : public App {
 public:
  ISGDScheduler() : App() { }
  virtual ~ISGDScheduler();
  virtual void process(const MessagePtr& msg);
  virtual void run();

 protected:
  // print the progress
  virtual void showProgress(
      double time, std::unordered_map<NodeID, SGDProgress>* progress);
  // merge the progress report from a computation node
  virtual void mergeProgress(const SGDProgress& src, SGDProgress* dst);
  MonitorMaster<SGDProgress> monitor_;

  WorkloadPool *workload_pool_ = nullptr;

  // display
  size_t num_ex_processed_ = 0;
  bool show_prog_head_ = true;
};

// the base class of a computation node
class ISGDCompNode : public App {
 public:
  ISGDCompNode() : App(), reporter_(SchedulerID()) { }
  virtual ~ISGDCompNode() { }

 protected:
  MonitorSlaver<SGDProgress> reporter_;
};

// a multithread minibatch reader
template <typename V>
class MinibatchReader {
 public:
  MinibatchReader() { }
  ~MinibatchReader() { }

  // read *minibatch_size* examples from file each time
  void setReader(const DataConfig& file, int minibatch_size, int data_buf = 1000) {
    reader_.init(file);
    minibatch_size_ = minibatch_size;
    data_prefetcher_.setCapacity(data_buf);
  }

  // features whose frequency <= freq is filtered by the countmin sketch with
  // parameter *n* and *k*
  void setFilter(size_t n, int k, int freq) {
    filter_.resize(n, k);
    key_freq_ = freq;
  }

  // start the reader thread
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

  // read a minibatch
  // *Y*: the *minibatch_size* x 1 label vector
  // *X*: the *minibatch_size* x p data matrix, all features are remapped to
  // continues id starting from 0
  // *key*: p length array contains the original feature id in the data
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

    // remap keys
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
