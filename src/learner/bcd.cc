#include "learner/bcd.h"
#include "util/resource_usage.h"
#include "util/split.h"
namespace PS {

void BCDScheduler::ProcessRequest(Message* request) {
  CHECK(request->task.has_bcd());
  auto bcd = request->task.bcd();
  if (bcd.cmd() == BCDCall::REQUEST_WORKLOAD) {
    Task req; req.mutable_bcd()->set_cmd(BCDCall::LOAD_DATA);
    CHECK(data_assigner_.next(req.mutable_bcd()->mutable_data()));
    Submit(req, request->sender);
  }
}

void BCDScheduler::ProcessResponse(Message* response) {
  const auto& task = response->task;
  if (!task.has_bcd()) return;

  if (task.bcd().cmd() == BCDCall::LOAD_DATA) {
    LoadDataResponse info;
    CHECK(info.ParseFromString(task.msg()));
    // LL << info.DebugString();
    g_train_info_ = mergeExampleInfo(g_train_info_, info.example_info());
    hit_cache_ += info.hit_cache();
    ++ load_data_;
  } else if (task.bcd().cmd() == BCDCall::EVALUATE_PROGRESS) {
    BCDProgress prog;
    CHECK(prog.ParseFromString(task.msg()));
    MergeProgress(task.bcd().iter(), prog);
  }
}

void BCDScheduler::LoadData() {
  // wait workers have load the data
  sys_.manager().WaitServersReady();
  sys_.manager().WaitWorkersReady();
  auto load_time = tic();
  int n = sys_.manager().num_workers();
  while (load_data_ < n) usleep(500);
  if (hit_cache_ > 0) {
    CHECK_EQ(hit_cache_, n) << "clear the local caches";
    NOTICE("Hit local caches for the training data");
  }
  NOTICE ("Loaded %lu examples in %g sec", g_train_info_.num_ex(), toc(load_time));
}

void BCDScheduler::PreprocesseData() {
  // preprocess the training data
  for (int i = 0; i < g_train_info_.slot_size(); ++i) {
    auto info = g_train_info_.slot(i);
    CHECK(info.has_id());
    if (info.id() == 0) continue;  // it's the label
    fea_grp_.push_back(info.id());
  }
  auto prep_time = tic();
  Task req; auto bcd = req.mutable_bcd();
  bcd->set_cmd(BCDCall::PREPROCESS_DATA);
  bcd->set_time(0);
  for (auto grp : fea_grp_) bcd->add_fea_grp(grp);
  bcd->set_hit_cache(hit_cache_ > 0);
  Wait(Submit(req, kCompGroup));

  NOTICE("Preprocessing is finished in %lf sec", toc(prep_time));
  if (bcd_conf_.tail_feature_freq()) {
    NOTICE("Features with frequency <= %d are filtered", bcd_conf_.tail_feature_freq());
  }
}

void BCDScheduler::DivideFeatureBlocks() {
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
      auto block = Range<Key>(info.min_key(), info.max_key()).EvenDivide(n, i);
      if (block.empty()) continue;
      fea_blk_.push_back(std::make_pair(info.id(), block));
    }
  }
  NOTICE("Features are partitioned into %ld blocks", fea_blk_.size());

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
    if (!hit_blk.empty()) {
      NOTICE("Prior feature groups: %s", join(hit_blk, ", ").c_str());
    }
    total_timer_.restart();
  }


void BCDScheduler::MergeProgress(int iter, const BCDProgress& recv) {
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

int BCDScheduler::SaveModel(const DataConfig& data) {
  Task task;
  task.mutable_bcd()->set_cmd(BCDCall::SAVE_MODEL);
  *task.mutable_bcd()->mutable_data() = data;
  return Submit(task, kCompGroup);
}

string BCDScheduler::ShowTime(int iter) {
  char buf[500];
  if (iter == -3) {
    snprintf(buf, 500, "|    time (sec.)");
  } else if (iter == -2) {
    snprintf(buf, 500, "|(app:min max) total");
  } else if (iter == -1) {
    snprintf(buf, 500, "+-----------------");
  } else {
    auto prog = g_progress_[iter];
    double ttl_t = prog.total_time() - (
        iter > 0 ? g_progress_[iter-1].total_time() : 0);
    int n = prog.busy_time_size();
    Eigen::ArrayXd busy_t(n);
    for (int i = 0; i < n; ++i) {
      busy_t[i] = prog.busy_time(i);
    }
    snprintf(buf, 500, "|%6.1f%6.1f%6.1f", busy_t.minCoeff(), busy_t.maxCoeff(), ttl_t);
  }
  return string(buf);
}

string BCDScheduler::ShowObjective(int iter) {
  char buf[500];
  if (iter == -3) {
    snprintf(buf, 500, "     |        training        |  sparsity ");
  } else if (iter == -2) {
    snprintf(buf, 500, "iter |  objective    relative |     |w|_0 ");
  } else if (iter == -1) {
    snprintf(buf, 500, " ----+------------------------+-----------");
  } else {
    auto prog = g_progress_[iter];
    snprintf(buf, 500, "%4d | %.5e  %.3e |%10lu ",
             iter, prog.objective(), prog.relative_obj(), (size_t)prog.nnz_w());
  }
  return string(buf);
}

}  // namespace PS
