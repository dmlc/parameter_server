#pragma once
#include "util/producer_consumer.h"
#include "learner/proto/sgd.pb.h"
#include "base/localizer.h"
#include "system/dist_monitor.h"
#include "system/postmaster.h"
#include "system/app.h"
namespace PS {

// stochastic gradient descent solver

class SGDScheduler : public App {
 public:
  SGDScheduler(const string& name)
      : App(name), monitor_(name+"_monitor", name) {
    using namespace std::placeholders;
    monitor_.setDataMerger(std::bind(&SGDScheduler::addProgress, this, _1, _2));

  }
  virtual ~SGDScheduler() { }

  void saveModel() {
    Task task;
    task.mutable_sgd()->set_cmd(SGDCall::SAVE_MODEL);
    port(kServerGroup)->submitAndWait(task);
  }

  void updateModel(const DataConfig& data) {
    monitor_.monitor(1, std::bind(&SGDScheduler::showProgress, this));
    // ask the servers to report the progress
    Task task;
    task.mutable_sgd()->set_cmd(SGDCall::UPDATE_MODEL);
    port(kServerGroup)->submitAndWait(task);
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

 protected:
  virtual void showProgress() {
    Lock l(progress_mu_);
    uint64 num_ex = 0, nnz_w = 0;
    SArray<double> objv;
    SArray<double> auc;
    for (const auto& it : recent_progress_) {
      auto& prog = it.second;
      num_ex += prog.num_examples_processed();
      nnz_w += prog.nnz();
      for (int i = 0; i < prog.objective_size(); ++i) {
        objv.pushBack(prog.objective(i));
      }
      for (int i = 0; i < prog.auc_size(); ++i) {
        auc.pushBack(prog.auc(i));
      }
    }
    recent_progress_.clear();
    num_ex_processed_ += num_ex;

    printf("%10lu examples, loss %.3e +- %.3e, auc %.4f +- %.4f, |w|_0 %8llu\n",
           num_ex_processed_ , objv.mean(), objv.std(),
           auc.mean(), auc.std(), nnz_w);
  }

  void addProgress(const NodeID& sender, const SGDProgress& prog) {
    Lock l(progress_mu_);
    recent_progress_.push_back(std::make_pair(sender, prog));
  }

  DistMonitor<SGDProgress> monitor_;
  std::vector<std::pair<NodeID, SGDProgress>> recent_progress_;
  std::mutex progress_mu_;
  size_t num_ex_processed_ = 0;

};

class SGDCompNode : public App {
 public:
  SGDCompNode(const string& name)
      : App(name),
        reporter_(name+"_monitor", name) { }
  virtual ~SGDCompNode() { }

 protected:
  DistMonitor<SGDProgress> reporter_;
};

class SGDServer : public SGDCompNode {
 public:
  SGDServer(const string& name) : SGDCompNode(name) { }
  virtual ~SGDServer() { }

  virtual void process(const MessagePtr& msg) {
    auto sgd = msg->task.sgd();
    if (sgd.cmd() == SGDCall::UPDATE_MODEL) {
      this->reporter_.reporter(
          this->schedulerID(), sgd.report_interval(), [this](SGDProgress* prog) {
            evaluate(prog);
        });
    } else if (sgd.cmd() == SGDCall::SAVE_MODEL) {
      saveModel();
    }
  }

  // virtual void updateModel() = 0;
  virtual void evaluate(SGDProgress* prog) =  0;
  virtual void saveModel() = 0;
};

template <typename Reader, typename Minibatch>
class SGDWorker : public SGDCompNode {
 public:
  SGDWorker(const string& name) : SGDCompNode(name) { }
  virtual ~SGDWorker() { }

  virtual void process(const MessagePtr& msg) {
    auto sgd = msg->task.sgd();
    if (sgd.cmd() == SGDCall::UPDATE_MODEL) {
      // start the progress reporter
      this->reporter_.reporter(
          this->schedulerID(), sgd.report_interval(), [this](SGDProgress* prog){
            Lock l(progress_mu_);
            *prog = progress_;
            progress_.Clear();
        });
      // start data prefecter thread
      Reader reader;
      reader.init(sgd.data());
      data_prefetcher_.setCapacity(10000);  // TODO set by config
      data_prefetcher_.startProducer(
          [this, &reader](Minibatch* data, size_t* size)->bool {
            if (!readMinibatch(reader, data)) return false;
            *size = data->size();
            return true;
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

// template<typename V>
// struct AdaGradEntry {
//   AdaGradEntry() { weight = 0; sum_sqr_grad = 1e-20; }
//   V weight;
//   V sum_sqr_grad;
//   static V eta = .1;
//   static V lambda = 1;
//   void get(char const* data) {
//     V grad = *((V*)data);
//     sum_sqr_grad += grad * grad;
//     V delta = eta * (grad / sqrt(sum_sqr_grad) + lambda * weight);
//     weight -= delta > 1.0 ? 1.0 : ( delta < -1.0 ? -1.0 : delta );
//   }
//   void set(char* data) {
//     *((V*)data) = weight;
//   }
// };

// template<typename V>
// class AdaGradUpdater : public KVStore<Key, AdaGradEntry<V>> {
// public:

// };


// template<typename V>
// using AdaGradUpdaterPtr = std::shared_ptr<AdaGradUpdater<V>>;

} // namespace PS
