#pragma once

#include "base/matrix.h"
#include "base/sparse_matrix.h"
#include "base/dense_matrix.h"
#include "base/shared_array_inl.h"
#include "util/recordio.h"
#include "proto/app.pb.h"
#include "proto/instance.pb.h"
#include "util/file.h"

namespace PS {

//// handle info
FeatureGroupInfo mergeFeatureGroupInfo(
    const FeatureGroupInfo& A, const FeatureGroupInfo& B);

InstanceInfo mergeInstanceInfo(
    const InstanceInfo& A, const InstanceInfo& B);

InstanceInfo readInstanceInfo(const DataConfig& config);

template<typename V>
MatrixInfo readMatrixInfo(const InstanceInfo& info, int i);

//// read from recordio file
// label, feature_group 1, feature_group 2, ...
// TODO do not support dense feature group yet...
template<typename V>
MatrixPtrList<V> readMatricesFromProto(const DataConfig& data);


//// read from binary file
template<typename V>
MatrixPtr<V> readMatrixFromBin(const std::string& file) {
  return readMatrixFromBin<V>(SizeR::all(), file);
}

template<typename V>
MatrixPtr<V> readMatrixFromBin(SizeR outer_range, const std::string& file);

template<typename V>
MatrixPtrList<V> readMatricesFromBin(
    SizeR outer_range, const std::vector<std::string>& files) {
  MatrixPtrList<V> res;
  for (auto& f : files) res.push_back(readMatrixFromBin<V>(outer_range, f));
  return res;
}

///// the main entry
template<typename V>
MatrixPtrList<V> readMatrices(const DataConfig& config);


} // namespace PS
