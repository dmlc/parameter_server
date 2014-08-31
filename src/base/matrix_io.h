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

// return A \union B
FeatureGroupInfo mergeFeatureGroupInfo(
    const FeatureGroupInfo& A, const FeatureGroupInfo& B);

// return A \union B
InstanceInfo mergeInstanceInfo(
    const InstanceInfo& A, const InstanceInfo& B);

// convert the i-th feature group info into matrix info
template<typename V>
MatrixInfo readMatrixInfo(const InstanceInfo& info, int i);

// Read from recordio files. Each instance is stored in a protobuf format and
// placed in a binary file one by one. Return two matrices, label vector and
// feature matrix
template<typename V>
bool readMatricesFromProto(const DataConfig& data, MatrixPtrList<V>* mat);

// Read from binary files, which are direct dumps of memory.
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

// Read from text files
template<typename V>
bool readMatricesFromText(const DataConfig& data, MatrixPtrList<V>* mat) {
  // TODO. The basic idea: use ParseText::toProto to convert text lines into
  // protobuf format (save them in disk if necessary), then add into matrix by
  // SArray::pushBack() (no need to know the size at the begining)
  //
  return false;
}

// Read matrices from local disk or hdfs in binary, protobuf, or text formats
template<typename V>
bool readMatrices(const DataConfig& data, MatrixPtrList<V>* mat);

// Read or die
template<typename V>
MatrixPtrList<V> readMatricesOrDie(const DataConfig& data);


} // namespace PS
