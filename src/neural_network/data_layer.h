#pragma once
#include "neural_network/layer.h"
#include "base/matrix_io.h"

namespace PS {
namespace NN {

template<typename V>
class DataLayer : public Layer<V>  {
 public:
  void init() {
    data_ = readMatrices<V>(cf_.data());
    CHECK_EQ(data_.size(), cf_.out_size());
    // TODO normalization

    size_t n = data_[0]->rows();
    for (size_t i = 0; ; ++i) {
      size_t size = cf_.minibatch_size();
      size = size == 0 ? n : size;
      SizeR batch = SizeR(0, size) * i;
      if (batch.end() > n) break;
      batches_.push_back(batch);
    }
  }

  // forward the data
  V forward() {
    out_args_.resize(data_.size());
    SizeR batch = batches_[rand() % batches_.size()];
    for (int i = 0; i < data_.size(); ++i) {
      out_args_.value = data_[i]->rowBlock(batch);
    }
    return 0;
  }

  // do nothing
  void backward() {}
 protected:
  MatrixPtrList<V> data_;
  std::vector<SizeR> batches_;
  size_t batch_id_ = 0;
};

} // namespace NN
} // namespace PS
