#pragma once

#include "util/common.h"

namespace PS {
namespace NN {

template<typename V> struct Blob;
template<typename V> using BlobPtr = std::shared_ptr<Blob<V>>;
template<typename V> using BlobPtrList = std::vector<BlobPtr<V>>;

// data blob
template<typename V>
struct Blob {
  MatrixPtr<V> value;
  MatrixPtr<V> gradient;
  MatrixPtr<V> hession;
};


} // namespace NN
} // namespace PS
