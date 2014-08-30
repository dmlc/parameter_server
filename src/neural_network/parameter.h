#pragma once

#include "util/common.h"
// #include <sparsehash/dense_hash_map>

namespace PS {
namespace NN {

template<typename V> struct Parameter;
template<typename V> using ParameterPtr = std::shared_ptr<Parameter<V>>;
template<typename V> using ParameterPtrList = std::vector<ParameterPtr<V>>;

// typedef google::dense_hash_map<Key, std::vector<V>>  SpaMatrix;
template<typename V>
struct Parameter {
  // Parameter() { }
  Parameter(const string& n)
      : name(n) { }
  string name;
  MatrixPtr<V> value;
  MatrixPtr<V> gradient;
  MatrixPtr<V> hession;

  // google::dense_hash_map<Key, std::vector<V>> spa_value;
  std::unordered_map<Key, std::vector<V>> spa_value;


};


} // namespace NN
} // namespace PS
