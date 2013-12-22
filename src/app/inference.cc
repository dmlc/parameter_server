#include "app/inference.h"
#include "util/rspmat.h"

namespace PS {

DEFINE_int32(max_push_delay, 0, "maximal push delay");
DEFINE_int32(max_pull_delay, 0, "maximal pull delay");
DEFINE_double(eta, .001, "learning rate");
DEFINE_string(train_data, "../data/rcv1.train", "training data");
DEFINE_string(test_data, "../data/rcv1.test", "test data");


void Inference::Init() {

  // a better way to get the range
  DataRange whole_data = RSpMat<>::RowSeg(FLAGS_train_data);

  postoffice_ = Postoffice::Instance();
  postoffice_->Init();
  postmaster_ = Postmaster::Instance();
  postmaster_->Register(this, whole_data);

  if (postmaster_->IamClient())
    data_range_ = postmaster_->GetWorkload(name_,
                                           postmaster_->my_uid())->data_range();
}

} // namespace PS
