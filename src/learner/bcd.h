#pragma once
#include "learner/proto/bcd.pb.h"
#include "data/slot_reader.h"
#include "data/common.h"
#include "system/postmaster.h"
#include "parameter/kv_buffered_vector.h"
namespace PS {
DECLARE_bool(verbose);
// block coordinate descent solver
class BCDCommon {
 public:
  BCDCommon(const BCDConfig& conf) : bcd_conf_(conf) {
    if (!bcd_conf_.has_local_cache()) {
      bcd_conf_.mutable_local_cache()->add_file("/tmp/bcd_");
    }
  }

  virtual ~BCDCommon() { }
 protected:
  // feature group
  std::vector<int> fea_grp_;
  const int time_ratio_ = 10;
  BCDConfig bcd_conf_;
};

class BCDScheduler : public App, public BCDCommon {
 public:
  BCDScheduler(const string& name, const BCDConfig& conf)
      : App(name), BCDCommon(conf) { }
  virtual ~BCDScheduler() { }

  void loadTrainingData(const DataConfig& train) {
    // ask the workers to load the data
    auto load_time = tic();
    auto confs = Postmaster::partitionData(train, sys_.yp().num_workers());
    std::vector<Task> loads(confs.size());
    for (int i = 0; i < confs.size(); ++i) {
      auto bcd = loads[i].mutable_bcd();
      bcd->set_cmd(BCDCall::LOAD_DATA);
      *bcd->mutable_data() = confs[i];
    }
    int hit_cache = 0;
    port(kWorkerGroup)->submitAndWait(loads, [this, &hit_cache](){
        LoadDataReturn info; CHECK(info.ParseFromString(exec_.lastRecvReply()));
        // LL << info.DebugString();
        g_train_info_ = mergeExampleInfo(g_train_info_, info.example_info());
        hit_cache += info.hit_cache();
      });
    if (hit_cache > 0) {
      CHECK_EQ(hit_cache, loads.size()) << "clear the local caches";
      LI << "Hit local caches for the training data";
    }
    LI << "Loaded " << g_train_info_.num_ex() << " examples in "
       << toc(load_time) << " sec";


    // preprocess the training data
    for (int i = 0; i < g_train_info_.slot_size(); ++i) {
      auto info = g_train_info_.slot(i);
      CHECK(info.has_id());
      if (info.id() == 0) continue;  // it's the label
      fea_grp_.push_back(info.id());
    }
    auto preprocess_time = tic();
    Task preprocess;
    auto prep_bcd = preprocess.mutable_bcd();
    prep_bcd->set_cmd(BCDCall::PREPROCESS_DATA);
    for (auto grp : fea_grp_) prep_bcd->add_fea_grp(grp);
    prep_bcd->set_hit_cache(hit_cache > 0);
    port(kCompGroup)->submitAndWait(preprocess);
    LI << "Preprocessing is finished in " << toc(preprocess_time) << " sec";
    if (bcd_conf_.tail_feature_freq()) {
      LI << "Features with frequency <= " << bcd_conf_.tail_feature_freq()
         << " are filtered";
    }
  }

  void divideFeatureBlocks() {
    // partition feature blocks
    for (int i = 0; i < g_train_info_.slot_size(); ++i) {
      auto info = g_train_info_.slot(i);
      CHECK(info.has_id());
      if (info.id() == 0) continue;  // it's the label
      CHECK(info.has_nnz_ele());
      CHECK(info.has_nnz_ex());
      double nnz_per_row = (double)info.nnz_ele() / (double)info.nnz_ex();
      int n = 1;  // number of blocks for a feature group
      if (nnz_per_row > 1 + 1e-6) {
        // only parititon feature group whose features are correlated
        n = std::max((int)std::ceil(nnz_per_row * bcd_conf_.feature_block_ratio()), 1);
      }
      for (int i = 0; i < n; ++i) {
        auto block = Range<Key>(info.min_key(), info.max_key()).evenDivide(n, i);
        if (block.empty()) continue;
        fea_blk_.push_back(std::make_pair(info.id(), block));
      }
    }
    LI << "Features are partitioned into " << fea_blk_.size() << " blocks";

    // a simple block order
    for (int i = 0; i < fea_blk_.size(); ++i) blk_order_.push_back(i);

    // blocks for important feature groups
    std::vector<string> hit_blk;
    for (int i = 0; i < bcd_conf_.prior_fea_group_size(); ++i) {
      int grp_id = bcd_conf_.prior_fea_group(i);
      std::vector<int> tmp;
      for (int k = 0; k < fea_blk_.size(); ++k) {
        if (fea_blk_[k].first == grp_id) tmp.push_back(k);
      }
      if (tmp.empty()) continue;
      hit_blk.push_back(std::to_string(grp_id));
      for (int j = 0; j < bcd_conf_.num_iter_for_prior_fea_group(); ++j) {
        if (bcd_conf_.random_feature_block_order()) {
          std::random_shuffle(tmp.begin(), tmp.end());
        }
        prior_blk_order_.insert(prior_blk_order_.end(), tmp.begin(), tmp.end());
      }
    }
    if (!hit_blk.empty()) LI << "Prior feature groups: " + join(hit_blk, ", ");
    total_timer_.restart();
  }

 protected:
  void mergeProgress(int iter) {
    BCDProgress recv;
    CHECK(recv.ParseFromString(exec_.lastRecvReply()));
    auto& p = g_progress_[iter];
    p.set_objective(p.objective() + recv.objective());
    p.set_nnz_w(p.nnz_w() + recv.nnz_w());

    if (recv.busy_time_size() > 0) p.add_busy_time(recv.busy_time(0));
    p.set_total_time(total_timer_.stop());
    total_timer_.start();
    p.set_relative_obj(iter==0 ? 1 : g_progress_[iter-1].objective()/p.objective() - 1);
    p.set_violation(std::max(p.violation(), recv.violation()));
    p.set_nnz_active_set(p.nnz_active_set() + recv.nnz_active_set());
  }

  void showTime(int iter) {
    if (iter == -3) {
      fprintf(stderr, "|    time (sec.)\n");
    } else if (iter == -2) {
      fprintf(stderr, "|(app:min max) total\n");
    } else if (iter == -1) {
      fprintf(stderr, "+-----------------\n");
    } else {
      auto prog = g_progress_[iter];
      double ttl_t = prog.total_time() - (
          iter > 0 ? g_progress_[iter-1].total_time() : 0);
      int n = prog.busy_time_size();
      Eigen::ArrayXd busy_t(n);
      for (int i = 0; i < n; ++i) {
        busy_t[i] = prog.busy_time(i);
      }
      fprintf(stderr, "|%6.1f%6.1f%6.1f\n", busy_t.minCoeff(), busy_t.maxCoeff(), ttl_t);
    }
  }

  void showObjective(int iter) {
    if (iter == -3) {
      fprintf(stderr, "     |        training        |  sparsity ");
    } else if (iter == -2) {
      fprintf(stderr, "iter |  objective    relative |     |w|_0 ");
    } else if (iter == -1) {
      fprintf(stderr, " ----+------------------------+-----------");
    } else {
      auto prog = g_progress_[iter];
      fprintf(stderr, "%4d | %.5e  %.3e |%10lu ",
              iter, prog.objective(), prog.relative_obj(), (size_t)prog.nnz_w());
    }
  }

  // progress of all iterations. The progress of all nodes are merged for every
  // iteration.
  std::map<int, BCDProgress> g_progress_;
  Timer total_timer_;
  // feature block info, format: pair<fea_grp_id, fea_range>
  typedef std::vector<std::pair<int, Range<Key>>> FeatureBlocks;
  FeatureBlocks fea_blk_;
  std::vector<int> blk_order_;
  std::vector<int> prior_blk_order_;
  // global data information
  ExampleInfo g_train_info_;
};

template <typename V>
class BCDServer : public App, public BCDCommon {
 public:
  BCDServer(const string& name, const BCDConfig& conf)
      : App(name), BCDCommon(conf), model_(name+"_model", name) { }
  virtual ~BCDServer() { }

  void process(const MessagePtr& msg) {
    CHECK(msg->task.has_bcd());
    auto bcd = msg->task.bcd();
    int time  = msg->task.time() * time_ratio_;
    switch (bcd.cmd()) {
      case BCDCall::PREPROCESS_DATA:
        preprocessData(time, bcd);
        break;
      case BCDCall::UPDATE_MODEL:
        updateModel(time, bcd);
        break;
      case BCDCall::SAVE_MODEL:
        CHECK(bcd.has_data());
        saveModel(bcd.data());
      case BCDCall::EVALUATE_PROGRESS: {
        BCDProgress prog; evaluate(&prog);
        sys_.replyProtocalMessage(msg, prog);
        break;
      }
      default: break;
    }
  }
 protected:
  virtual void updateModel(int time, const BCDCall& call) = 0;
  virtual void evaluate(BCDProgress* prog) = 0;

  virtual void preprocessData(int time, const BCDCall& call) {
    int grp_size = call.fea_grp_size();
    fea_grp_.clear();
    for (int i = 0; i < grp_size; ++i) {
      fea_grp_.push_back(call.fea_grp(i));
    }
    bool hit_cache = call.hit_cache();
    // filter tail keys
    for (int i = 0; i < grp_size; ++i, time += time_ratio_) {
      if (hit_cache) continue;
      model_.waitInMsg(kWorkerGroup, time);
      model_.finish(kWorkerGroup, time+1);
    }
    for (int i = 0; i < grp_size; ++i, time += time_ratio_) {
      // wait untill received all keys from workers
      model_.waitInMsg(kWorkerGroup, time);
      // initialize the weight
      int chl = fea_grp_[i];
      model_.clearTailFilter(chl);
      model_.value(chl).resize(model_.key(chl).size());
      model_.value(chl).setValue(bcd_conf_.init_w());
      model_.finish(kWorkerGroup, time+1);
    }
  }

  void saveModel(const DataConfig& output) {
    if (output.format() == DataConfig::TEXT) {
      CHECK(output.file_size());
      std::string file = output.file(0) + "_" + myNodeID();
      std::ofstream out(file); CHECK(out.good());
      for (int grp : fea_grp_) {
        auto key = model_.key(grp);
        auto value = model_.value(grp);
        CHECK_EQ(key.size(), value.size());
        // TODO use the model_file in msg
        for (size_t i = 0; i < key.size(); ++i) {
          double v = value[i];
          if (v != 0 && !(v != v)) out << key[i] << "\t" << v << "\n";
        }
      }
      LI << myNodeID() << " written the model to " << file;
    }
  }
  KVBufferedVector<Key, V> model_;
};

template <typename V>
class BCDWorker : public App, public BCDCommon {
 public:
  BCDWorker(const string& name, const BCDConfig& conf)
      : App(name), BCDCommon(conf), model_(name+"_model", name) { }
  virtual ~BCDWorker() { }

  void process(const MessagePtr& msg) {
    CHECK(msg->task.has_bcd());
    auto bcd = msg->task.bcd();
    int time  = msg->task.time() * time_ratio_;
    switch (bcd.cmd()) {
      case BCDCall::LOAD_DATA: {
        LoadDataReturn ret;
        int hit_cache = 0;
        CHECK(bcd.has_data());
        loadData(bcd.data(), ret.mutable_example_info(), &hit_cache);
        ret.set_hit_cache(hit_cache);
        sys_.replyProtocalMessage(msg, ret);
        break;
      }
      case BCDCall::PREPROCESS_DATA:
        preprocessData(time, bcd);
        break;
      case BCDCall::UPDATE_MODEL:
        computeGradient(time, bcd, msg);
        msg->finished = false;  //
        break;
      case BCDCall::EVALUATE_PROGRESS: {
        BCDProgress prog; evaluate(&prog);
        sys_.replyProtocalMessage(msg, prog);
        break;
      }
      default: break;
    }
  }
 protected:
  virtual void computeGradient(int time, const BCDCall& bcd, MessagePtr msg) = 0;
  virtual void evaluate(BCDProgress* prog) = 0;

  void loadData(const DataConfig& data, ExampleInfo* info, int *hit_cache) {
    *hit_cache = dataCache("train", true);
    if (!(*hit_cache)) {
      slot_reader_.init(data, bcd_conf_.local_cache());
      slot_reader_.read(info);
    }
  }

  virtual void preprocessData(int time, const BCDCall& call) {
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
    for (int i = 0; i < grp_size; ++i, time += time_ratio_) {
      if (hit_cache) continue;
      int grp = fea_grp_[i];

      // find all unique keys with their count in feature group i
      SArray<Key> uniq_key;
      SArray<uint8> key_cnt;
      Localizer<Key, double> *localizer = new Localizer<Key, double>();

      if (FLAGS_verbose) {
        LI << "counting unique key [" << i + 1 << "/" << grp_size << "]";
      }
      localizer->countUniqIndex(slot_reader_.index(grp), &uniq_key, &key_cnt);

      if (FLAGS_verbose) {
        LI << "counted unique key [" << i + 1 << "/" << grp_size << "]";
      }

      // push to servers
      MessagePtr count(new Message(kServerGroup, time));
      count->setKey(uniq_key);
      count->addValue(key_cnt);
      count->task.set_key_channel(grp);
      count->addFilter(FilterConfig::KEY_CACHING);
      auto tail = model_.set(count)->mutable_tail_filter();
      tail->set_insert_count(true);
      tail->set_countmin_k(bcd_conf_.countmin_k());
      tail->set_countmin_n((int)(uniq_key.size()*bcd_conf_.countmin_n_ratio()));
      CHECK_EQ(time, model_.push(count));

      // pull filtered keys after the servers have aggregated all counts
      MessagePtr filter(new Message(kServerGroup, time+2, time+1));
      filter->setKey(uniq_key);
      filter->task.set_key_channel(grp);
      filter->addFilter(FilterConfig::KEY_CACHING);
      model_.set(filter)->mutable_tail_filter()->set_query_key(
          bcd_conf_.tail_feature_freq());
      filter->fin_handle = [this, grp, localizer, i, grp_size]() mutable {
        // localize the training matrix
        if (FLAGS_verbose) {
          LI << "started remapIndex [" << i + 1 << "/" << grp_size << "]";
        }

        auto X = localizer->remapIndex(grp, model_.key(grp), &slot_reader_);
        delete localizer;
        slot_reader_.clear(grp);
        if (!X) return;

        if (FLAGS_verbose) {
          LI << "finished remapIndex [" << i + 1 << "/" << grp_size << "]";
          LI << "started toColMajor [" << i + 1 << "/" << grp_size << "]";
        }

        if (bcd_conf_.has_feature_block_ratio()) X = X->toColMajor();

        if (FLAGS_verbose) {
          LI << "finished toColMajor [" << i + 1 << "/" << grp_size << "]";
        }

        { Lock l(mu_); X_[grp] = X; }
      };
      CHECK_EQ(time+2, model_.pull(filter));
      pull_time[i] = time + 2;

      // wait if necessary
      if (i >= max_parallel) {
        model_.waitOutMsg(kServerGroup, pull_time[i-max_parallel]);
      }
    }

    // push the filtered keys to severs to let severs build the key maps. because
    // of the key_caching, these keys are not sent, so the communication cost is
    // little. then pull the intialial model value back
    std::vector<std::promise<void>> wait_dual(grp_size);
    for (int i = 0; i < grp_size; ++i, time += time_ratio_) {
      // wait if necessary
      if (!hit_cache && i >= grp_size - max_parallel) {
        model_.waitOutMsg(kServerGroup, pull_time[i]);
      }

      // push the filtered keys to servers
      MessagePtr push_key(new Message(kServerGroup, time));
      int grp = fea_grp_[i];
      push_key->setKey(model_.key(grp));
      push_key->task.set_key_channel(grp);
      push_key->addFilter(FilterConfig::KEY_CACHING);
      CHECK_EQ(time, model_.push(push_key));

      // fetch the initial value of the model
      MessagePtr pull_val(new Message(kServerGroup, time+2, time+1));
      pull_val->setKey(model_.key(grp));
      pull_val->task.set_key_channel(grp);
      pull_val->addFilter(FilterConfig::KEY_CACHING)->set_clear_cache_if_done(true);
      pull_val->fin_handle = [this, grp, time, &wait_dual, i]() {
        size_t n = model_.key(grp).size();
        if (n) { // otherwise received(time+2) may return error
          // intialize the local cache of the model
          auto init_w = model_.received(time+2);
          CHECK_EQ(init_w.second.size(), 1);
          CHECK_EQ(n, init_w.first.size());
          model_.value(grp) = init_w.second[0];

          // set the local variable
          auto X = X_[grp]; CHECK(X);
          if (dual_.empty()) {
            dual_.resize(X->rows(), 0);
          } else {
            CHECK_EQ(dual_.size(), X->rows());
          }
          if (bcd_conf_.init_w().type() != ParameterInitConfig::ZERO) {
            dual_.eigenVector() = *X * model_.value(grp).eigenVector();
          }
        }
        wait_dual[i].set_value();
      };
      CHECK_EQ(time+2, model_.pull(pull_val));
      pull_time[i] = time + 2;
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
    dataCache("train", false);
  }

  bool dataCache(const string& name, bool load) {
    return false;
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
  }

  SlotReader slot_reader_;

  // training data
  std::map<int, MatrixPtr<V>> X_;  //slot_id versus X
  MatrixPtr<double> y_;
  // dual_ = X * w
  SArray<double> dual_;

  std::mutex mu_;

  KVBufferedVector<Key, V> model_;
};

} // namespace PS
