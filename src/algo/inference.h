#pragma once
#include "util/common.h"
#include "util/eigen3.h"
#include "system/postmaster.h"
#include "system/postoffice.h"

namespace PS {

DECLARE_int32(max_push_delay);
DECLARE_int32(max_pull_delay);
DECLARE_double(eta);
DECLARE_string(train_data);
DECLARE_string(test_data);

struct Progress {
  Progress(double v = 0, int64 e = 0) : objv(v), err(e) { }
  Progress& operator+=(const Progress& rhs) {
    objv += rhs.objv; err += rhs.err;
 // nnz += rhs.nnz;
    return *this;
  }
  Progress operator+(const Progress& rhs) {
    Progress res = *this; res += rhs; return res;
  }
  string ToString() { return StrCat(objv, ", ", err); }
  double objv;
  // int64 nnz;
  int64 err;
};

static size_t ClassifyError(const DVec& true_y, const DVec& pred_y) {
  CHECK_EQ(true_y.size(),pred_y.size());
  size_t err = 0;
  for (size_t i = 0; i < true_y.size(); ++i)
    if (true_y[i] * (pred_y[i]-.5) < 0) ++err;
  return err;
}

class Inference {
 public:
  Inference(string name) : name_(name) { }
  // the typical steps to init an inference algorithm
  // 1. get the training data statistics, such as #traning sample
  // 2. initial all client nodes, and assign data partitions
  // 3. load the data partition
  virtual void Init();
  virtual void Run() {
    if(postmaster_->IamClient())
      Client();
    else
      Server();
  }
  // virtual void LoadData() = 0;
  virtual void Client() = 0;
  virtual void Server() = 0;

  string SName() { return StrCat(postmaster_->MyNode().ShortName(), ": "); }
  // void Register(Container *ctr) { postmaster_->Register(ctr, this); }
  const string& name() { return name_; }
 protected:
  // the name of the running inference.
  string name_;

  // the samples this client reads.
  DataRange data_range_;

  // statistics about the whole training data
  // size_t num_total_nnz_;
  // size_t num_total_sample_;
  // size_t num_total_feature_;

  Postmaster *postmaster_;
  Postoffice *postoffice_;
 private:
};

} // namespace PS
