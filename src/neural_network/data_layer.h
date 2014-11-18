#pragma once
#include "neural_network/layer.h"
#include "base/matrix_io_inl.h"

namespace PS {
namespace NN {

template<typename V>
class DataLayer : public Layer<V>  {
 public:
  void init() {
    // data_ = readMatricesOrDie<V>(cf_.data());
    CHECK_EQ(data_.size(), cf_.out_size());
    // TODO normalization

    size_t n = data_[0]->rows();
    for (size_t i = 0; ; ++i) {
      size_t size = cf_.minibatch_size();
      size = size == 0 ? n : size;
      SizeR batch = SizeR(0, size) + i * size;
      if (batch.end() > n) break;
      batches_.push_back(batch);
    }

    for (size_t i = 0; i < data_[0]->cols(); ++i) {
      auto col = data_[0]->colBlock(SizeR(i,i+1))->eigenArray();
      col -= col.mean();
      V v = col.matrix().norm();
      if (v != 0) col = col / v * sqrt((V)n);
      // LL << data_[0]->colBlock(SizeR(i,i+1))->eigenArray().matrix().squaredNorm();
    }

    CHECK_EQ(data_.size(), 2);
    cf_.set_size(data_[0]->cols());
  }

  // forward the data
  V forward() {
    CHECK_EQ(out_args_.size(), data_.size());
    SizeR batch = batches_[rand() % batches_.size()];
    // SizeR batch = batches_[0];
    for (int i = 0; i < data_.size(); ++i) {
      if (data_[i]->colMajor()) {
        data_[i] = data_[i]->toRowMajor();
        // LL << data_[i]->debugString();
      }
      out_args_[i]->value = data_[i]->rowBlock(batch);
      // LL << out_args_[i]->value->debugString();
    }

    return 0;
  }

  // do nothing
  void backward() {}
 protected:
  USING_LAYER;
  MatrixPtrList<V> data_;
  std::vector<SizeR> batches_;
  size_t batch_id_ = 0;
};

} // namespace NN
} // namespace PS
