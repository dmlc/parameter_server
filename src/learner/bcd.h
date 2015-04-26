/**
 * @file   bcd.h
 * @brief  The interface for a block coordinate descent solver
 */
#pragma once
#include "learner/proto/bcd.pb.h"
#include "system/assigner.h"
#include "data/slot_reader.h"
#include "data/common.h"
#include "util/localizer.h"
#include "parameter/kv_vector.h"
namespace PS {
#if USE_S3
bool s3file(const std::string& name);
std::string s3Prefix(const std::string& path);
std::string s3Bucket(const std::string& path);
std::string s3FileUrl(const std::string& path);
#endif // USE_S3


/**
 * @brief The scheduler.
 */
class BCDScheduler : public App {
 public:
  BCDScheduler(const BCDConfig& conf) : App(), bcd_conf_(conf) { }
  virtual ~BCDScheduler() { }

  virtual void ProcessRequest(Message* request);
  virtual void ProcessResponse(Message* response);
  virtual void Run() {
    LoadData();
    PreprocesseData();
    DivideFeatureBlocks();
  }

 protected:
  /**
   * @brief issues model saving tasks to workers
   */
  int SaveModel(const DataConfig& data);
  /**
   * @brief returns the time string
   */
  std::string ShowTime(int iter);
  /**
   * @brief returns the objective string
   */
  std::string ShowObjective(int iter);

  // progress of all iterations. The progress of all nodes are merged for every
  // iteration.
  std::map<int, BCDProgress> g_progress_;
  // feature block info, format: pair<fea_grp_id, fea_range>
  typedef std::vector<std::pair<int, Range<Key>>> FeatureBlocks;
  FeatureBlocks fea_blk_;
  std::vector<int> blk_order_;
  std::vector<int> prior_blk_order_;
  // global data information
  ExampleInfo g_train_info_;
  BCDConfig bcd_conf_;
  std::vector<int> fea_grp_;
  DataAssigner data_assigner_;
 private:
  /**
   * @brief waits util all workers finished data loading
   */
  void LoadData();

  /**
   * @brief issues data preprocessing tasks to workers
   */
  void PreprocesseData();

  /**
   * @brief divide feature into blocks
   */
  void DivideFeatureBlocks();

  /**
   * @brief merge the progress of all nodes at iteration iter
   */
  void MergeProgress(int iter, const BCDProgress& recv);

  int hit_cache_ = 0;
  int load_data_ = 0;
  Timer total_timer_;
};

/**
 * @brief The iterface of a computation node. One must implement the virtual
 * functions defined here.
 */
template <typename V>
class BCDCompNode : public App {
 public:
  BCDCompNode(const BCDConfig& conf) : bcd_conf_(conf), model_(true) { }
  virtual ~BCDCompNode() { }
  virtual void ProcessRequest(Message* request) {
    const auto& task = request->task;
    CHECK(task.has_bcd());
    int time = task.bcd().time();
    switch (task.bcd().cmd()) {
      case BCDCall::PREPROCESS_DATA:
        PreprocessData(time, request);
        break;
      case BCDCall::UPDATE_MODEL:
        Update(time, request);
        break;
      case BCDCall::EVALUATE_PROGRESS: {
        BCDProgress prog; Evaluate(&prog);
        string str; CHECK(prog.SerializeToString(&str));
        Task res; res.set_msg(str);
        res.mutable_bcd()->set_iter(task.bcd().iter());
        res.mutable_bcd()->set_cmd(BCDCall::EVALUATE_PROGRESS);
        Reply(request, res);
      } break;
      default: break;
    }
  }
 protected:
  /**
   * @brief Update the model
   *
   * @param time the timestamp
   * @param msg the request from the scheduler
   */
  virtual void Update(int time, Message* msg) = 0;
  /**
   * @brief Evaluate the progress
   *
   * @param prog the progress
   */
  virtual void Evaluate(BCDProgress* prog) = 0;
  /**
   * @brief Preprocesse the training data.
   *
   * Tranpose the training data into column-major format, and also build the key
   * maping from global uint64 key to local continous key to fast the local
   * computation.
   *
   * @param time the timestamp
   * @param msg the request from the scheduler
   */
  virtual void PreprocessData(int time, Message* msg) = 0;

  const int time_ratio_ = 3;
  // feature group
  std::vector<int> fea_grp_;
  BCDConfig bcd_conf_;
  KVVector<Key, V> model_;
};

#define USING_BCD_COMP_NODE                     \
  using BCDCompNode<V>::fea_grp_;               \
  using BCDCompNode<V>::model_;                 \
  using BCDCompNode<V>::bcd_conf_

/**
 * @brief A server node
 *
 */
template <typename V>
class BCDServer : public BCDCompNode<V> {
 public:
  BCDServer(const BCDConfig& conf)
      : BCDCompNode<V>(conf) { }
  virtual ~BCDServer() { }

  virtual void ProcessRequest(Message* request) {
    BCDCompNode<V>::ProcessRequest(request);
    auto bcd = request->task.bcd();
    if (bcd.cmd() == BCDCall::SAVE_MODEL) {
      CHECK(bcd.has_data());
      SaveModel(bcd.data());
    }
  }
 protected:
  virtual void PreprocessData(int time, Message* request) {
    auto call = request->task.bcd();
    int grp_size = call.fea_grp_size();
    fea_grp_.clear();
    for (int i = 0; i < grp_size; ++i) {
      fea_grp_.push_back(call.fea_grp(i));
    }
    bool hit_cache = call.hit_cache();
    // filter tail keys
    for (int i = 0; i < grp_size; ++i, time += 3) {
      if (hit_cache) continue;
      model_.WaitReceivedRequest(time, kWorkerGroup);
      model_.FinishReceivedRequest(time+1, kWorkerGroup);
    }
    for (int i = 0; i < grp_size; ++i, time += 3) {
      // wait until received all keys from workers
      model_.WaitReceivedRequest(time, kWorkerGroup);
      // initialize the weight
      auto& grp = model_[fea_grp_[i]];
      grp.value.resize(grp.key.size());
      grp.value.SetValue(bcd_conf_.init_w());
      model_.FinishReceivedRequest(time+1, kWorkerGroup);
    }
    model_.ClearFilter();
  }

  void SaveModel(const DataConfig& output) {
    if (output.format() == DataConfig::TEXT) {
      CHECK(output.file_size());
      std::string file = output.file(0) + "_" + MyNodeID();
#if USE_S3
      std::string s3_file;
      if (s3file(file)) {
        s3_file=file;
        // create a local model dir
        file=s3Prefix(s3_file);
      }
#endif // USE_S3
      if (!dirExists(getPath(file))) {
        createDir(getPath(file));
      }
      std::ofstream out(file); CHECK(out.good());
      for (int grp : fea_grp_) {
        auto key = model_[grp].key;
        auto value = model_[grp].value;
        CHECK_EQ(key.size(), value.size());
        // TODO use the model_file in msg
        for (size_t i = 0; i < key.size(); ++i) {
          double v = value[i];
          if (v != 0 && !(v != v)) out << key[i] << "\t" << v << "\n";
        }
      }
#if USE_S3
      if (s3file(s3_file)) {
        // upload model
        std::string cmd = "curl -s '"+s3FileUrl(s3_file)+"?Content-Length="
        +std::to_string(File::size(file))+"&x-amz-acl=public-read' --upload-file "+file;
        system(cmd.c_str());
        // remove local model
        cmd="rm -rf "+file;
        system(cmd.c_str());
        LI << MyNodeID() << " written the model to " << s3_file;
      } else {
        LI << MyNodeID() << " written the model to " << file;
      }
#else
      LI << MyNodeID() << " written the model to " << file;
#endif // USE_S3
    }
  }
  USING_BCD_COMP_NODE;
};

template <typename V>
class BCDWorker : public BCDCompNode<V> {
 public:
  BCDWorker(const BCDConfig& conf)
      : BCDCompNode<V>(conf) {
    if (!bcd_conf_.has_local_cache()) {
      bcd_conf_.mutable_local_cache()->add_file("/tmp/bcd_");
    }
  }
  virtual ~BCDWorker() { }

  virtual void Run() {
    Task task;
    task.mutable_bcd()->set_cmd(BCDCall::REQUEST_WORKLOAD);
    this->Submit(task, SchedulerID());
  }

  virtual void ProcessRequest(Message* request) {
    BCDCompNode<V>::ProcessRequest(request);
    auto bcd = request->task.bcd();
    if (bcd.cmd() == BCDCall::LOAD_DATA) {
      CHECK(bcd.has_data());
      LoadDataResponse ret;
      int hit_cache = 0;
      LoadData(bcd.data(), ret.mutable_example_info(), &hit_cache);

      ret.set_hit_cache(hit_cache);
      string str; CHECK(ret.SerializeToString(&str));
      Task res; res.set_msg(str);
      res.mutable_bcd()->set_cmd(BCDCall::LOAD_DATA);
      this->Reply(request, res);
    }
  }
 protected:
  void LoadData(const DataConfig& data, ExampleInfo* info, int *hit_cache) {
    *hit_cache = DataCache("train", true);
    if (!(*hit_cache)) {
      slot_reader_.Init(searchFiles(data), bcd_conf_.local_cache());
      slot_reader_.Read(info);
    }
  }

  virtual void PreprocessData(int time, Message *msg) {
    const auto& call = msg->task.bcd();
    int grp_size = call.fea_grp_size();
    fea_grp_.clear();
    for (int i = 0; i < grp_size; ++i) {
      fea_grp_.push_back(call.fea_grp(i));
    }
    bool hit_cache = call.hit_cache();
    int max_parallel = std::max(
        1, bcd_conf_.max_num_parallel_groups_in_preprocessing());

    // filter keys whose occurance <= bcd_conf_.tail_feature_freq()
    std::vector<int> pull_time(grp_size);
    for (int i = 0; i < grp_size; ++i, time += 3) {
      if (hit_cache) continue;
      pull_time[i] = FilterTailFeatures(time, i);

      // wait if necessary to reduce memory usage
      if (i >= max_parallel) {
        model_.Wait(pull_time[i-max_parallel]);
      }
    }

    // push the filtered keys to severs then pull the intialial model value back
    std::vector<std::promise<void>> wait_dual(grp_size);
    for (int i = 0; i < grp_size; ++i, time += 3) {
      // wait if necessary
      if (!hit_cache && i >= grp_size - max_parallel) {
        model_.Wait(pull_time[i]);
      }
      InitModel(time, fea_grp_[i], wait_dual[i]);
    }

    // load the label if necessary
    if (!hit_cache) {
      y_ = MatrixPtr<double>(new DenseMatrix<double>(
          slot_reader_.info<double>(0), slot_reader_.value<double>(0)));
    }

    // wait until all weight pull finished
    for (int i = 0; i < grp_size; ++i) {
      wait_dual[i].get_future().wait();
    }
    DataCache("train", false);
  }
 private:
  int FilterTailFeatures(int time, int i) {
    int grp_size = fea_grp_.size();
    int grp = fea_grp_[i];
    // find all unique keys with their count in feature group i
    SArray<Key> uniq_key;
    SArray<uint8> key_cnt;
    Localizer<Key, double> *localizer = new Localizer<Key, double>();

    VLOG(1) << "counting unique key [" << i + 1 << "/" << grp_size << "]";
    localizer->CountUniqIndex(slot_reader_.index(grp), &uniq_key, &key_cnt);
    VLOG(1) << "finished counting [" << i + 1 << "/" << grp_size << "]";

    // push key and count to servers
    Task push = Parameter::Request(grp, time, {}, bcd_conf_.comm_filter());
    auto tail = push.mutable_param()->mutable_tail_filter();
    tail->set_insert_count(true);
    tail->set_countmin_k(bcd_conf_.countmin_k());
    tail->set_countmin_n((int)(uniq_key.size()*bcd_conf_.countmin_n_ratio()));
    Message push_msg(push, kServerGroup);
    push_msg.set_key(uniq_key);
    push_msg.add_value(key_cnt);
    model_.Push(&push_msg);

    // pull filtered keys after the servers have aggregated all counts
    Task pull = Parameter::Request(grp, time+2, {time+1}, bcd_conf_.comm_filter());
    tail = pull.mutable_param()->mutable_tail_filter();
    tail->set_freq_threshold(bcd_conf_.tail_feature_freq());
    Message pull_msg(pull, kServerGroup);
    pull_msg.set_key(uniq_key);
    pull_msg.callback = [this, grp, localizer, i, grp_size]() mutable {
      // localize the training matrix
      VLOG(1) << "remap index [" << i + 1 << "/" << grp_size << "]";
      auto X = localizer->RemapIndex(grp, model_[grp].key, &slot_reader_);
      delete localizer;
      slot_reader_.clear(grp);
      if (!X) return;
      VLOG(1) << "finished [" << i + 1 << "/" << grp_size << "]";

      VLOG(1) << "transpose to column major [" << i + 1 << "/" << grp_size << "]";
      X = X->toColMajor();
      VLOG(1) << "finished [" << i + 1 << "/" << grp_size << "]";

      { Lock l(mu_); X_[grp] = X; }
    };
    return model_.Pull(&pull_msg);
  }

  void InitModel(int time, int grp, std::promise<void>& wait) {
    // push the filtered keys to let servers build the key maps. when the
    // key_caching fitler is used, the communication cost will be little
    model_.Push(Parameter::Request(grp, time, {}, bcd_conf_.comm_filter()),
                model_[grp].key);

    // fetch the initial value of the model
    model_.Pull(
        Parameter::Request(grp, time+2, {time+1}, bcd_conf_.comm_filter()),
        model_[grp].key, [this, grp, time, &wait]() {
          size_t n = model_[grp].key.size();
          if (n) { // otherwise buffer(time+2) may return error
            // intialize the local cache of the model
            auto init_w = model_.buffer(time+2);
            CHECK_EQ(init_w.values.size(), 1);
            CHECK_EQ(init_w.idx_range.size(), n);
            CHECK_EQ(init_w.channel, grp);

            model_[grp].value = init_w.values[0];

            // set the local variable
            auto X = X_[grp]; CHECK(X);
            if (dual_.empty()) {
              dual_.resize(X->rows(), 0);
            } else {
              CHECK_EQ(dual_.size(), X->rows());
            }
            if (bcd_conf_.init_w().type() != ParamInitConfig::ZERO) {
              dual_.EigenVector() = *X * model_[grp].value.EigenVector();
            }
          }
          wait.set_value();
        });
  }

  bool DataCache(const string& name, bool load) {
    // TODO
    return false;
  }

  SlotReader slot_reader_;

 protected:
  // <slot id, feature matrix>
  std::unordered_map<int, MatrixPtr<V>> X_;
  // label
  MatrixPtr<V> y_;
  // dual_ = X * w
  SArray<V> dual_;

  std::mutex mu_;

  USING_BCD_COMP_NODE;
};

} // namespace PS

  // bool DataCache(const string& name, bool load) {
    // // load / save label
    // auto cache = conf_.local_cache();
    // auto y_conf = ithFile(cache, 0, name + "_label_" + myNodeID());
    // if (load) {
    //   MatrixPtrList<double> y_list;
    //   if (!readMatrices<double>(y_conf, &y_list) || !(y_list.size()==1)) return false;
    //   y_ = y_list[0];
    // } else {
    //   if (!y_->writeToBinFile(y_conf.file(0))) return false;
    // }
    // // load / save feature groups
    // string info_file = cache.file(0) + name + "_" + myNodeID() + ".info";
    // InstanceInfo info = y_->info().ins_info();
    // if (load && !readFileToProto(info_file, &info)) return false;
    // InstanceInfo new_info;
    // for (int i = 0; i < info.fea_grp_size(); ++i) {
    //   int id = info.fea_grp(i).grp_id();
    //   string x_name = name + "_fea_grp_" + std::to_string(id) + "_" + myNodeID();
    //   auto x_conf = ithFile(cache, 0, x_name);
    //   string key_name = name + "_key_" + std::to_string(id) + "_" + myNodeID();
    //   auto key_conf = ithFile(cache, 0, key_name);
    //   if (load) {
    //     MatrixPtrList<double> x_list;
    //     if (!readMatrices<double>(x_conf, &x_list) || !(x_list.size()==1)) return false;
    //     X_[id] = x_list[0];
    //     if (!w_->key(id).readFromFile(SizeR(0, X_[id]->cols()), key_conf)) return false;
    //   } else {
    //     if (!X_[id]) continue;
    //     if (w_->key(id).empty()) LL << id << " " << X_[id]->debugString();
    //     if (!(X_[id]->writeToBinFile(x_conf.file(0))
    //           && w_->key(id).writeToFile(key_conf.file(0)))) return false;
    //     *new_info.add_fea_grp() = info.fea_grp(i);
    //   }
    // }
    // if (!load && !writeProtoToASCIIFile(new_info, info_file)) return false;
    // return true;
  // }
