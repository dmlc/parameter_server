#pragma once

#include "util/common.h"

namespace PS {
namespace NN {

template<typename V> struct Parameter;
template<typename V> using ParameterPtr = std::shared_ptr<Parameter<V>>;
template<typename V> using ParameterPtrList = std::vector<ParameterPtr<V>>;

// data blob
template<typename V>
struct Parameter {
  // Parameter() { }
  Parameter(const string& n)
      : name(n) { }
  string name;
  MatrixPtr<V> value;
  MatrixPtr<V> gradient;
  MatrixPtr<V> hession;
};


} // namespace NN
} // namespace PS
