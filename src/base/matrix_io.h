#pragma once

#include "base/matrix.h"
#include "base/sparse_matrix.h"
#include "base/dense_matrix.h"
#include "base/shared_array_inl.h"
#include "util/recordio.h"
#include "proto/app.pb.h"
#include "proto/instance.pb.h"

namespace PS {

// return A \union B
// InstanceInfo mergeInstanceInfo(const InstanceInfo& A, const InstanceInfo& B);

// convert the feature group *grp_id* info into matrix info
template<typename V>
MatrixInfo readMatrixInfo(const InstanceInfo& info, int i);

// // Read from recordio files. Each instance is stored in a protobuf format and
// // placed in a binary file one by one. Return two matrices, label vector and
// // feature matrix
// template<typename V>
// bool readMatricesFromProto(const DataConfig& data, MatrixPtrList<V>* mat);

// // Read from binary files, which are direct dumps of memory.
// template<typename V>
// bool readMatricesFromBin(const DataConfig& data, MatrixPtrList<V>* mat);

// // Read from text files
// // TODO only support read sparse data now
// template<typename V>
// bool readMatricesFromText(const DataConfig& data, MatrixPtrList<V>* mat);

// // Read matrices from local disk or hdfs in binary, protobuf, or text formats,
// // return a list of matrices: label, fea_grp_0, fea_grp_1, ...
// template<typename V>
// bool readMatrices(const DataConfig& data, MatrixPtrList<V>* mat);

// // Read or die
// template<typename V>
// MatrixPtrList<V> readMatricesOrDie(const DataConfig& data);


} // namespace PS
