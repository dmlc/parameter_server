#pragma once
#include "learner/sgd_comp_node.h"
#include "data/stream_reader.h"
#include "base/localizer.h"
#include "util/producer_consumer.h"
namespace PS {

template <typename value_t>
class SGDWorker : public SGDCompNode {
 public:
  SGDWorker(const string& name) : SGDCompNode(name) { }
  virtual ~SGDWorker() { }

  struct Minibatch {
    MatrixPtr<value_t> label;
    LocalizerPtr<Key, value_t> localizer;
    int batch_id;
    int pull_time;
  };

  virtual void evaluateProgress(SGDProgress* prog) {
    Lock l(progress_mu_);
    *prog = progress_;
    progress_.Clear();
  }

  virtual void process(const MessagePtr& msg) {
    auto sgd = get(msg);
    if (sgd.cmd() == SGDCall::UPDATE_MODEL) {
      startReporter(sgd.report_interval());
      startDataPrefetcher(sgd.data());
      Minibatch data;
      while (data_prefetcher_.pop(&data)) {
        computeGradient(data);
      }
    }
  }

  void startDataPrefetcher(const DataConfig& data, size_t buf_size_in_mb = 10000) {
    reader_.init(data);
    data_prefetcher_.setCapacity(buf_size_in_mb);
    data_prefetcher_.startProducer(
        [this](Minibatch* data, size_t* size)->bool {
          bool ret = readMinibatch(reader_, data);
          *size = 1;  // TODO should = label->memSize() + localizer->memSize();
          return ret;
        });
  }

  virtual bool readMinibatch(StreamReader<value_t>& reader, Minibatch* data) = 0;
  virtual void computeGradient(Minibatch& data) = 0;

 protected:
  ProducerConsumer<Minibatch> data_prefetcher_;
  StreamReader<value_t> reader_;
};

} // namespace PS
