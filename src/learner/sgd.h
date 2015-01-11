#pragma once
#include "util/producer_consumer.h"
#include "learner/sgd.pb.h"
#include "data/stream_reader.h"
#include "base/localizer.h"
#include "system/dist_monitor.h"
#include "system/app.h"
namespace PS {

// static SGDCall getSGDCall(const MessageCPtr& msg) {
//   CHECK_EQ(msg->task.type(), Task::CALL_CUSTOMER);
//   CHECK(msg->task.has_sgd());
//   return msg->task.sgd();
// }

// static SGDCall* setSGDCall(Task *task) {
//   task->set_type(Task::CALL_CUSTOMER);
//   return task->mutable_sgd();
// }

// static Task newSGDTask(SGDCall::Command cmd) {
//   Task task; setSGDCall(&task)->set_cmd(cmd);
//   return task;
// }

template <typename V>
struct SparseMinibatch {
  size_t size() {
    return label->memSize() + localizer->memSize();
  }
  MatrixPtr<V> label;
  LocalizerPtr<Key, V> localizer;
  int batch_id;
  int pull_time;
};

class SGDScheduler : public App {
 public:
  SGDScheduler(const string& name)
      : App(name), monitor_(name+"_monitor", name) {
    using namespace std::placeholders;
    monitor_.setDataMerger(std::bind(&SGDScheduler::addProgress, this, _1, _2));
  }
  virtual ~SGDScheduler() { }

  virtual void run() {
    // divide data

    monitor_.monitor(1, std::bind(&SGDScheduler::showProgress, this));

    Task update; // = newTask(SGDCall::UPDATE_MODEL);
    taskpool(kCompGroup)->submitAndWait(update);

    Task save_model; // = newTask(SGDCall::SAVE_MODEL);
    taskpool(kServerGroup)->submitAndWait(save_model);
  }

  virtual void process(const MessagePtr& msg) {

  }

  virtual void showProgress() {
    // TODO

  }
  void addProgress(const NodeID& sender, const SGDProgress& prog) {
    Lock l(progress_mu_);
    recent_progress_.push_back(std::make_pair(sender, prog));
  }

 protected:
  DistMonitor<SGDProgress> monitor_;
  std::vector<std::pair<NodeID, SGDProgress>> recent_progress_;
  std::mutex progress_mu_;
};

template <typename Model>
class SGDCompNode : public App {
 public:
  SGDCompNode(const string& name)
      : App(name),
        reporter_(name+"_monitor", name),
        model_(name+"_model", name) { }
  virtual ~SGDCompNode() { }

 protected:
  DistMonitor<SGDProgress> reporter_;
  Model model_;
};

template <typename Model>
class SGDServer : public SGDCompNode<Model> {
 public:
  SGDServer(const string& name) : SGDCompNode<Model>(name) { }
  virtual ~SGDServer() { }

  virtual void process(const MessagePtr& msg) {
    auto sgd = msg->task.sgd();
    if (sgd.cmd() == SGDCall::UPDATE_MODEL) {
      this->reporter_.reporter(this->schedulerID(), 1, [this](SGDProgress* prog){
          // this->model_.evaluate(prog);
        });
    } else if (sgd.cmd() == SGDCall::SAVE_MODEL) {
      saveModel();
    }
  }

  // virtual void updateModel() = 0;
  virtual void saveModel() = 0;
};

template <typename Reader, typename Minibatch, typename Model>
class SGDWorker : public SGDCompNode<Model> {
 public:
  SGDWorker(const string& name) : SGDCompNode<Model>(name) { }
  virtual ~SGDWorker() { }

  virtual void process(const MessagePtr& msg) {
    auto sgd = msg->task.sgd();
    if (sgd.cmd() == SGDCall::UPDATE_MODEL) {
      // start the progress reporter
      this->reporter_.reporter(this->schedulerID(), 1, [this](SGDProgress* prog){
          Lock l(progress_mu_);
          *prog = progress_;
          progress_.Clear();
        });
      // start data prefecter thread
      Reader reader;
      reader.init(sgd.data());
      data_prefetcher_.setCapacity(10000);
      data_prefetcher_.startProducer(
          [this, &reader](Minibatch* data, size_t* size)->bool {
            bool ret = readMinibatch(reader, data);
            *size = data->size();
            return ret;
        });
      // compute gradient
      Minibatch data;
      while (data_prefetcher_.pop(&data)) {
        computeGradient(data);
      }
    }
  }

  virtual bool readMinibatch(Reader& reader, Minibatch* data) = 0;
  virtual void computeGradient(Minibatch& data) = 0;

 protected:
  ProducerConsumer<Minibatch> data_prefetcher_;
  SGDProgress progress_;
  std::mutex progress_mu_;
};

template<typename V>
struct AdaGradEntry {
  AdaGradEntry() { weight = 0; sum_sqr_grad = 1e-20; }
  V weight;
  V sum_sqr_grad;
  static V eta = .1;
  static V lambda = 1;
  void get(char const* data) {
    V grad = *((V*)data);
    sum_sqr_grad += grad * grad;
    V delta = eta * (grad / sqrt(sum_sqr_grad) + lambda * weight);
    weight -= delta > 1.0 ? 1.0 : ( delta < -1.0 ? -1.0 : delta );
  }
  void set(char* data) {
    *((V*)data) = weight;
  }
};

// template<typename V>
// class AdaGradUpdater : public KVStore<Key, AdaGradEntry<V>> {
// public:

// };


// template<typename V>
// using AdaGradUpdaterPtr = std::shared_ptr<AdaGradUpdater<V>>;

} // namespace PS
