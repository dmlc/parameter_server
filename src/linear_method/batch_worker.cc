#include "linear_method/batch_worker.h"
#include "util/split.h"
#include "base/matrix_io_inl.h"
#include "base/localizer.h"
#include "base/sparse_matrix.h"
#include "data/common.h"
namespace PS {
DECLARE_bool(verbose);
namespace LM {

void BatchWorker::init() {
  CompNode::init();
  model_ = KVBufferedVectorPtr<Key, double>(new KVBufferedVector<Key, double>());
  REGISTER_CUSTOMER(app_cf_.parameter_name(0), model_);
}

void BatchWorker::loadData(ExampleInfo* info, int *hit_cache) {
  *hit_cache = loadCache("train");
  if (!(*hit_cache)) {
    CHECK(conf_.has_local_cache());
    slot_reader_.init(conf_.training_data(), conf_.local_cache());
    slot_reader_.read(info);
  }
}

void BatchWorker::preprocessData(const MessagePtr& msg) {
  int time = msg->task.time() * k_time_ratio_;
  auto cmd = get(msg);
  int grp_size = cmd.fea_grp_size();
  fea_grp_.clear();
  for (int i = 0; i < grp_size; ++i) {
    fea_grp_.push_back(cmd.fea_grp(i));
  }
  bool hit_cache = cmd.hit_cache();
  int max_parallel = std::max(
      1, conf_.solver().max_num_parallel_groups_in_preprocessing());

  auto& hb = sys_.hb();
  // filter keys whose occurance <= conf_.solver().tail_feature_freq()
  std::vector<int> pull_time(grp_size);
  for (int i = 0; i < grp_size; ++i, time += k_time_ratio_) {
    if (hit_cache) continue;
    int grp = fea_grp_[i];

    // find all unique keys with their count in feature group i
    SArray<Key> uniq_key;
    SArray<uint8> key_cnt;
    Localizer<Key, double> *localizer = new Localizer<Key, double>();

    if (FLAGS_verbose) {
      LI << "counting unique key [" << i + 1 << "/" << grp_size << "]";
    }
    hb.startTimer(HeartbeatInfo::TimerType::BUSY);
    localizer->countUniqIndex(slot_reader_.index(grp), &uniq_key, &key_cnt);
    hb.stopTimer(HeartbeatInfo::TimerType::BUSY);

    if (FLAGS_verbose) {
      LI << "counted unique key [" << i + 1 << "/" << grp_size << "]";
    }

    // push to servers
    MessagePtr count(new Message(kServerGroup, time));
    count->setKey(uniq_key);
    count->addValue(key_cnt);
    count->task.set_key_channel(grp);
    count->addFilter(FilterConfig::KEY_CACHING);
    auto tail = model_->set(count)->mutable_tail_filter();
    tail->set_insert_count(true);
    tail->set_countmin_k(conf_.solver().countmin_k());
    tail->set_countmin_n((int)(uniq_key.size()*conf_.solver().countmin_n_ratio()));
    CHECK_EQ(time, model_->push(count));

    // pull filtered keys after the servers have aggregated all counts
    MessagePtr filter(new Message(kServerGroup, time+2, time+1));
    filter->setKey(uniq_key);
    filter->task.set_key_channel(grp);
    filter->addFilter(FilterConfig::KEY_CACHING);
    model_->set(filter)->mutable_tail_filter()->set_query_key(
        conf_.solver().tail_feature_freq());
    filter->fin_handle = [this, grp, localizer, i, grp_size]() mutable {
      // localize the training matrix
      if (FLAGS_verbose) {
        LI << "started remapIndex [" << i + 1 << "/" << grp_size << "]";
      }

      auto& hb = sys_.hb();
      hb.startTimer(HeartbeatInfo::TimerType::BUSY);
      auto X = localizer->remapIndex(grp, model_->key(grp), &slot_reader_);
      delete localizer;
      slot_reader_.clear(grp);
      hb.stopTimer(HeartbeatInfo::TimerType::BUSY);
      if (!X) return;

      if (FLAGS_verbose) {
        LI << "finished remapIndex [" << i + 1 << "/" << grp_size << "]";
        LI << "started toColMajor [" << i + 1 << "/" << grp_size << "]";
      }

      hb.startTimer(HeartbeatInfo::TimerType::BUSY);
      if (conf_.solver().has_feature_block_ratio()) X = X->toColMajor();
      hb.stopTimer(HeartbeatInfo::TimerType::BUSY);

      if (FLAGS_verbose) {
        LI << "finished toColMajor [" << i + 1 << "/" << grp_size << "]";
      }

      { Lock l(mu_); X_[grp] = X; }
    };
    CHECK_EQ(time+2, model_->pull(filter));
    pull_time[i] = time + 2;

    // wait if necessary
    if (i >= max_parallel) {
      model_->waitOutMsg(kServerGroup, pull_time[i-max_parallel]);
    }
  }

  // push the filtered keys to severs to let severs build the key maps. because
  // of the key_caching, these keys are not sent, so the communication cost is
  // little. then pull the intialial model value back
  std::vector<std::promise<void>> wait_dual(grp_size);
  for (int i = 0; i < grp_size; ++i, time += k_time_ratio_) {
    // wait if necessary
    if (!hit_cache && i >= grp_size - max_parallel) {
      model_->waitOutMsg(kServerGroup, pull_time[i]);
    }

    // push the filtered keys to servers
    MessagePtr push_key(new Message(kServerGroup, time));
    int grp = fea_grp_[i];
    push_key->setKey(model_->key(grp));
    push_key->task.set_key_channel(grp);
    push_key->addFilter(FilterConfig::KEY_CACHING);
    CHECK_EQ(time, model_->push(push_key));

    // fetch the initial value of the model
    MessagePtr pull_val(new Message(kServerGroup, time+2, time+1));
    pull_val->setKey(model_->key(grp));
    pull_val->task.set_key_channel(grp);
    pull_val->addFilter(FilterConfig::KEY_CACHING)->set_clear_cache_if_done(true);
    pull_val->fin_handle = [this, grp, time, &wait_dual, i]() {
      size_t n = model_->key(grp).size();
      if (n) { // otherwise received(time+2) may return error
        // intialize the local cache of the model
        auto init_w = model_->received(time+2);
        CHECK_EQ(init_w.second.size(), 1);
        CHECK_EQ(n, init_w.first.size());
        model_->value(grp) = init_w.second[0];

        // set the local variable
        auto X = X_[grp]; CHECK(X);
        if (dual_.empty()) {
          dual_.resize(X->rows(), 0);
        } else {
          CHECK_EQ(dual_.size(), X->rows());
        }
        if (conf_.init_w().type() != ParameterInitConfig::ZERO) {
          dual_.eigenVector() = *X * model_->value(grp).eigenVector();
        }
      }
      wait_dual[i].set_value();
    };
    CHECK_EQ(time+2, model_->pull(pull_val));
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
  saveCache("train");
}

bool BatchWorker::dataCache(const string& name, bool load) {
  if (!conf_.has_local_cache()) return false;
  return false;  // FIXME
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


// void BatchWorker::computeGradient(const MessagePtr& msg) {
  // FIXME a very old version
  // int time = msg->task.time() * 10;
  // Range<Key> global_range(msg->task.risk().key());
  // auto local_range = w_->localRange(global_range);

  // if (exec_.isWorker()) {
  //   auto X = X_->colBlock(local_range);

  //   SArrayList<double> local_grads(2);
  //   local_grads[0].resize(local_range.size());
  //   local_grads[1].resize(local_range.size());
  //   AggGradLearnerArg arg;
  //   {
  //     Lock l(mu_);
  //     busy_timer_.start();
  //     learner_->compute({y_, X, dual_.matrix()}, arg, local_grads);
  //     busy_timer_.stop();
  //   }

  //   msg->finished = false;
  //   auto sender = msg->sender;
  //   auto task = msg->task;
  //   w_->roundTripForWorker(time, global_range, local_grads,
  //                          [this, X, local_range, sender, task] (int time) {
  //       Lock l(mu_);
  //       busy_timer_.start();

  //       if (!local_range.empty()) {
  //         auto data = w_->received(time);

  //         CHECK_EQ(data.size(), 1);
  //         CHECK_EQ(local_range, data[0].first);
  //         auto new_w = data[0].second;

  //         auto delta = new_w.eigenVector() - w_->segment(local_range).eigenVector();
  //         dual_.eigenVector() += *X * delta;
  //         w_->segment(local_range).eigenVector() = new_w.eigenVector();
  //       }

  //       busy_timer_.stop();

  //       taskpool(sender)->finishIncomingTask(task.time());
  //       sys_.reply(sender, task);
  //       // LL << myNodeID() << " done " << d.task.time();
  //     });
  // } else {
  //   // aggregate local gradients, then update model
  //   w_->roundTripForServer(time, global_range, [this, local_range] (int time) {
  //       SArrayList<double> aggregated_gradient;
  //       for (auto& d : w_->received(time)) {
  //         CHECK_EQ(local_range, d.first);
  //         aggregated_gradient.push_back(d.second);
  //       }
  //       AggGradLearnerArg arg;
  //       arg.set_learning_rate(XXXX.learning_rate().eta());
  //       learner_->update(aggregated_gradient, arg, w_->segment(local_range));
  //     });
  // }
// }

} // namespace LM
} // namespace PS
