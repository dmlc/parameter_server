#pragma once

#include "util/common.h"

namespace PS {
namespace NN {

template<typename V> struct Argument;
template<typename V> using ArgumentPtr = std::shared_ptr<Argument<V>>;
template<typename V> using ArgumentPtrList = std::vector<ArgumentPtr<V>>;

template<typename V>
struct Argument {
  MatrixPtr<V> value;
  MatrixPtr<V> gradient;
  MatrixPtr<V> hession;
};


} // namespace NN
} // namespace PS
