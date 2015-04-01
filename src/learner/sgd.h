/**
 * @file   sgd.h
 * @brief  The interface for a stochastic gradient descent solver
 *
 */
#pragma once
#include "ps.h"
#include "util/producer_consumer.h"
#include "learner/proto/sgd.pb.h"
#include "system/monitor.h"
#include "system/assigner.h"
#include "data/stream_reader.h"
#include "util/localizer.h"
#include "filter/frequency_filter.h"
#include "learner/workload_pool.h"
namespace PS {

/**
 * @brief The base class of a scheduler node
 */
class ISGDScheduler : public App {
 public:
  ISGDScheduler() : App() { }
  virtual ~ISGDScheduler();

  virtual void Run();
  virtual void ProcessResponse(Message* response);
  virtual void ProcessRequest(Message* request);
 protected:
  // print the progress
  virtual void ShowProgress(
      double time, std::unordered_map<NodeID, SGDProgress>* progress);
  // merge the progress report from a computation node
  virtual void MergeProgress(const SGDProgress& src, SGDProgress* dst);

  void SendWorkload(const NodeID& recver);
  MonitorMaster<SGDProgress> monitor_;

  WorkloadPool *workload_pool_ = nullptr;

  // display
  size_t num_ex_processed_ = 0;
  bool show_prog_head_ = true;
};

/**
 * @brief The base class of a computation node
 */
class ISGDCompNode : public App {
 public:
  ISGDCompNode() : App(), reporter_(SchedulerID()) { }
  virtual ~ISGDCompNode() { }

 protected:
  MonitorSlaver<SGDProgress> reporter_;
};

/**
 * @brief A multithread minibatch reader
 * @tparam V the value type
 */
template <typename V>
class MinibatchReader {
 public:
  MinibatchReader() { }
  ~MinibatchReader() { }

  /**
   * @brief set the text reader
   *
   * @param file data file
   * @param minibatch_size # of examples
   * @param data_buf in MB
   */
  void InitReader(const DataConfig& file, int minibatch_size, int data_buf = 1000) {
    reader_.init(file);
    minibatch_size_ = minibatch_size;
    data_prefetcher_.setCapacity(data_buf);
  }

  /**
   * @brief tails features are filtered by the countmin sketch
   *
   * @param n countmin sketch parameter
   * @param k countmin sketch parameter
   * @param freq frequency threshold
   */
  void InitFilter(size_t n, int k, int freq) {
    filter_.Resize(n, k);
    key_freq_ = freq;
  }

  /**
   * @brief Start the reader thread
   */
  void Start() {
    data_prefetcher_.startProducer(
        [this](MatrixPtrList<V>* data, size_t* size)->bool {
          bool ret = reader_.readMatrices(minibatch_size_, data);
          for (const auto& mat : *data) {
            *size += mat->memSize();
          }
          return ret;
        });
  }

  /**
   * @brief Reads a minibatch
   *
   * @param Y the *minibatch_size* x 1 label vector
   * @param X the *minibatch_size* x p data matrix, all features are remapped to
   * continues id starting from 0 to p-1
   * @param key p length array contains the original feature id in the data
   *
   * @return false if end of file
   */
  bool Read(MatrixPtr<V>& Y, MatrixPtr<V>& X, SArray<Key>& key) {
    MatrixPtrList<V> data;
    if (!data_prefetcher_.pop(&data)) return false;
    CHECK_EQ(data.size(), 2);
    Y = data[0];

    // localizer
    SArray<Key> uniq_key;
    SArray<uint8> key_cnt;
    Localizer<Key, V> localizer;
    localizer.CountUniqIndex(data[1], &uniq_key, &key_cnt);

    // filter keys
    filter_.InsertKeys(uniq_key, key_cnt);
    key = filter_.QueryKeys(uniq_key, key_freq_);

    // remap keys
    X = localizer.RemapIndex(key);
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
