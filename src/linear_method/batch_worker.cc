#include "util/split.h"
#include "base/matrix_io_inl.h"
#include "base/localizer.h"
#include "base/sparse_matrix.h"
#include "data/common.h"

bool BatchSolver::dataCache(const string& name, bool load) {
  if (!conf_.has_local_cache()) return false;
  return false;  // FIXME
  // load / save label
  auto cache = conf_.local_cache();
  auto y_conf = ithFile(cache, 0, name + "_label_" + myNodeID());
  if (load) {
    MatrixPtrList<double> y_list;
    if (!readMatrices<double>(y_conf, &y_list) || !(y_list.size()==1)) return false;
    y_ = y_list[0];
  } else {
    if (!y_->writeToBinFile(y_conf.file(0))) return false;
  }
  // load / save feature groups
  string info_file = cache.file(0) + name + "_" + myNodeID() + ".info";
  InstanceInfo info = y_->info().ins_info();
  if (load && !readFileToProto(info_file, &info)) return false;
  InstanceInfo new_info;
  for (int i = 0; i < info.fea_grp_size(); ++i) {
    int id = info.fea_grp(i).grp_id();
    string x_name = name + "_fea_grp_" + std::to_string(id) + "_" + myNodeID();
    auto x_conf = ithFile(cache, 0, x_name);
    string key_name = name + "_key_" + std::to_string(id) + "_" + myNodeID();
    auto key_conf = ithFile(cache, 0, key_name);
    if (load) {
      MatrixPtrList<double> x_list;
      if (!readMatrices<double>(x_conf, &x_list) || !(x_list.size()==1)) return false;
      X_[id] = x_list[0];
      if (!w_->key(id).readFromFile(SizeR(0, X_[id]->cols()), key_conf)) return false;
    } else {
      if (!X_[id]) continue;
      if (w_->key(id).empty()) LL << id << " " << X_[id]->debugString();
      if (!(X_[id]->writeToBinFile(x_conf.file(0))
            && w_->key(id).writeToFile(key_conf.file(0)))) return false;
      *new_info.add_fea_grp() = info.fea_grp(i);
    }
  }
  if (!load && !writeProtoToASCIIFile(new_info, info_file)) return false;
  return true;
}

int BatchSolver::loadData(ExampleInfo* info) {
  bool hit_cache = loadCache("train");
  if (!hit_cache) {
    CHECK(conf_.has_local_cache());
    slot_reader_.init(conf_.training_data(), conf_.local_cache());
    slot_reader_.read(info);
  }
  return hit_cache;
}

void BatchSolver::preprocessData(const MessageCPtr& msg) {
  int time = msg->task.time() * kPace;
  int grp_size = get(msg).fea_grp_size();
  fea_grp_.clear();
  for (int i = 0; i < grp_size; ++i) fea_grp_.push_back(get(msg).fea_grp(i));
  bool hit_cache = get(msg).hit_cache();

  int max_parallel = std::max(1, conf_.solver().max_num_parallel_groups_in_preprocessing());

  if (IamWorker()) {
    std::vector<int> pull_time(grp_size);
    for (int i = 0; i < grp_size; ++i, time += kPace) {
      if (hit_cache) continue;
      int grp = fea_grp_[i];

      // Time 0: send all unique keys with their count to servers
      SArray<Key> uniq_key;
      SArray<uint32> key_cnt;
      // Localizer
      Localizer<Key, double> *localizer = new Localizer<Key, double>();

      if (FLAGS_verbose) {
        LI << "counting unique key [" << i + 1 << "/" << grp_size << "]";
      }

      this->sys_.hb().startTimer(HeartbeatInfo::TimerType::BUSY);
      localizer->countUniqIndex(slot_reader_.index(grp), &uniq_key, &key_cnt);
      this->sys_.hb().stopTimer(HeartbeatInfo::TimerType::BUSY);

      if (FLAGS_verbose) {
        LI << "counted unique key [" << i + 1 << "/" << grp_size << "]";
      }

      MessagePtr count(new Message(kServerGroup, time));
      count->setKey(uniq_key);
      count->addValue(key_cnt);
      count->task.set_key_channel(grp);
      count->addFilter(FilterConfig::KEY_CACHING);
      auto arg = w_->set(count);
      arg->set_insert_key_freq(true);
      arg->set_countmin_k(conf_.solver().countmin_k());
      arg->set_countmin_n((int)(uniq_key.size()*conf_.solver().countmin_n_ratio()));
      CHECK_EQ(time, w_->push(count));

      // time 2: pull filered keys
      MessagePtr filter(new Message(kServerGroup, time+2, time+1));
      filter->setKey(uniq_key);
      filter->task.set_key_channel(grp);
      filter->addFilter(FilterConfig::KEY_CACHING)->set_clear_cache_if_done(true);
      w_->set(filter)->set_query_key_freq(conf_.solver().tail_feature_freq());
      filter->fin_handle = [this, grp, localizer, i, grp_size]() mutable {
        // localize the training matrix
        if (FLAGS_verbose) {
          LI << "started remapIndex [" << i + 1 << "/" << grp_size << "]";
        }

        this->sys_.hb().startTimer(HeartbeatInfo::TimerType::BUSY);
        auto X = localizer->remapIndex(grp, w_->key(grp), &slot_reader_);
        delete localizer;
        slot_reader_.clear(grp);
        this->sys_.hb().stopTimer(HeartbeatInfo::TimerType::BUSY);
        if (!X) return;

        if (FLAGS_verbose) {
          LI << "finished remapIndex [" << i + 1 << "/" << grp_size << "]";
          LI << "started toColMajor [" << i + 1 << "/" << grp_size << "]";
        }

        this->sys_.hb().startTimer(HeartbeatInfo::TimerType::BUSY);
        if (conf_.solver().has_feature_block_ratio()) X = X->toColMajor();
        this->sys_.hb().stopTimer(HeartbeatInfo::TimerType::BUSY);

        if (FLAGS_verbose) {
          LI << "finished toColMajor [" << i + 1 << "/" << grp_size << "]";
        }

        { Lock l(mu_); X_[grp] = X; }
      };
      CHECK_EQ(time+2, w_->pull(filter));
      pull_time[i] = time + 2;

      // wait
      if (!hit_cache && i >= max_parallel) w_->waitOutMsg(kServerGroup, pull_time[i-max_parallel]);
    }

    for (int i = 0; i < grp_size; ++i, time += kPace) {
      // wait
      if (!hit_cache && i >= grp_size - max_parallel) w_->waitOutMsg(kServerGroup, pull_time[i]);

      // time 0: push the filtered keys to servers
      MessagePtr push_key(new Message(kServerGroup, time));
      int grp = fea_grp_[i];
      push_key->setKey(w_->key(grp));
      push_key->task.set_key_channel(grp);
      push_key->addFilter(FilterConfig::KEY_CACHING);
      CHECK_EQ(time, w_->push(push_key));

      // time 2: fetch initial value of w_
      MessagePtr pull_val(new Message(kServerGroup, time+2, time+1));
      pull_val->setKey(w_->key(grp));
      pull_val->task.set_key_channel(grp);
      push_key->addFilter(FilterConfig::KEY_CACHING)->set_clear_cache_if_done(true);
      pull_val->wait = true;
      CHECK_EQ(time+2, w_->pull(pull_val));
      pull_time[i] = time + 2;
      if (pull_val->key.empty()) continue; // otherwise received(time+2) will return error

      auto init_w = w_->received(time+2);
      CHECK_EQ(init_w.second.size(), 1);
      CHECK_EQ(w_->key(grp).size(), init_w.first.size());
      w_->value(grp) = init_w.second[0];

      // set the local variable
      auto X = X_[grp];
      if (!X) continue;
      if (dual_.empty()) {
        dual_.resize(X->rows());
        dual_.setZero();
      } else {
        CHECK_EQ(dual_.size(), X->rows());
      }
      if (conf_.init_w().type() != ParameterInitConfig::ZERO) {
        dual_.eigenVector() = *X * w_->value(grp).eigenVector();
      }
    }
    // the label
    if (!hit_cache) {
      y_ = MatrixPtr<double>(new DenseMatrix<double>(
          slot_reader_.info<double>(0), slot_reader_.value<double>(0)));
      // CHECK_EQ(y_->value().size(), info->num_ex());
    }
    // wait until all weight pull finished
    for (int i = 0; i < grp_size; ++i) {
      w_->waitOutMsg(kServerGroup, pull_time[i]);
    }
    saveCache("train");
  } else {
    for (int i = 0; i < grp_size; ++i, time += kPace) {
      if (hit_cache) continue;
      w_->waitInMsg(kWorkerGroup, time);
      w_->finish(kWorkerGroup, time+1);
    }
    for (int i = 0; i < grp_size; ++i, time += kPace) {
      w_->waitInMsg(kWorkerGroup, time);
      int chl = fea_grp_[i];
      w_->keyFilter(chl).clear();
      w_->value(chl).resize(w_->key(chl).size());
      w_->value(chl).setValue(conf_.init_w());
      w_->finish(kWorkerGroup, time+1);
    }
  }
}
