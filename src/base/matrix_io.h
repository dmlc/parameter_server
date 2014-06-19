#pragma once

#include "base/matrix.h"
#include "base/sparse_matrix.h"
#include "base/dense_matrix.h"
#include "base/shared_array_io.h"
#include "util/recordio.h"
#include "proto/config.pb.h"
#include "proto/instance.pb.h"
#include "util/file.h"

namespace PS {

template<typename V>
struct FillSparseMatrix {
  MatrixInfo info;
  SArray<size_t> offset;
  SArray<uint64> index;
  SArray<V> value;
  uint64 offset_pos;
  uint64 index_pos;
  uint64 value_pos;
  bool binary;
};

// TODO check optional
static FeatureGroupInfo
mergeFeatureGroupInfo(const FeatureGroupInfo& A, const FeatureGroupInfo& B) {
  CHECK_EQ(A.group_id(), B.group_id());
  CHECK_EQ(A.type(), B.type());
  auto C = A;
  C.set_feature_begin(std::min(A.feature_begin(), B.feature_begin()));
  C.set_feature_end(std::max(A.feature_end(), B.feature_end()));
  if (A.has_num_entries() && B.has_num_entries())
    C.set_num_entries(A.num_entries() + B.num_entries());
  if (A.has_num_instances() && B.has_num_instances())
    C.set_num_instances(A.num_instances() + B.num_instances());
  return C;
}

static InstanceInfo
mergeInstanceInfo(const InstanceInfo& A, const InstanceInfo& B) {
  CHECK_EQ(A.label_type(), B.label_type());
  CHECK_EQ(A.feature_type(), B.feature_type());

  InstanceInfo C;
  C.set_label_type(A.label_type());
  C.set_num_instances(A.num_instances() + B.num_instances());
  C.set_feature_begin(std::min(A.feature_begin(), B.feature_begin()));
  C.set_feature_end(std::max(A.feature_end(), B.feature_end()));

  CHECK_EQ(A.feature_group_info_size(), B.feature_group_info_size());
  for (int i = 0; i < A.feature_group_info_size(); ++i) {
    *C.add_feature_group_info() = mergeFeatureGroupInfo(
        A.feature_group_info(i), B.feature_group_info(i));
  }
  return C;
}

// template<typename V>
// MatrixInfo readMatrixInfo(const InstanceInfo g) {
//   MatrixInfo f;
//   // if (g.feature_type() == FeatureGroupInfo::DENSE)
//   //   f.set_type(MatrixInfo::DENSE);
//   // else if (g.feature_type() == FeatureGroupInfo::SPARSE)
//   //   f.set_type(MatrixInfo::SPARSE);
//   // else if (g.feature_type() == FeatureGroupInfo::SPARSE_BINARY)
//   //   f.set_type(MatrixInfo::SPARSE_BINARY);
//   // f.set_row_major(true);
//   // f.set_id(g.group_id());
//   // f.mutable_row()->set_begin(0);
//   // f.mutable_row()->set_end(g.num_instances());
//   // f.mutable_col()->set_begin(g.feature_begin());
//   // f.mutable_col()->set_end(g.feature_end());
//   // f.set_nnz(g.num_entries());
//   // f.set_sizeof_index(sizeof(uint64));
//   // f.set_sizeof_value(sizeof(V));
//   // f.set_nnz_per_row((double) f.nnz() / (double) g.num_instances());
//   return f;
// }

// label, feature_group 1, feature_group 2, ...
// TODO do not support dense feature group yet...
template<typename V>
MatrixPtrList<V> readMatricesFromProto(const std::vector<std::string>& files) {
  // load info
  InstanceInfo info;
  for (int i = 0; i < files.size(); ++i) {
    InstanceInfo f; ReadFileToProtoOrDie(files[i]+".info", &f);
    info = i == 0 ? f : mergeInstanceInfo(info, f);
  }
  // // LL << info.DebugString();

  // allocate data
  bool binary = info.feature_type() == FeatureGroupInfo::SPARSE_BINARY;
  SArray<V> label(info.num_instances());
  SArray<size_t> offset(info.num_instances()+1);
  offset[0] = 0;
  SArray<uint64> index(info.num_entries());
  SArray<V> value;
  if (!binary) value.resize(info.num_entries());

  uint64 offset_pos = 0;
  uint64 index_pos = 0;
  uint64 value_pos = 0;
  uint64 label_pos = 0;

  // file data
  for (auto f : files) {
    File* in = File::OpenOrDie(f+".recordio", "r");
    RecordReader reader(in);
    Instance record;
    while (reader.ReadProtocolMessage(&record)) {
      label[label_pos++] = record.label();
      int n = record.feature_id_size();
      for (int i = 0; i < n; ++i) {
        index[index_pos++] = record.feature_id(i);
        if (!binary) value[value_pos++] = record.value(i);
      }
      offset[offset_pos+1] = offset[offset_pos];
      offset_pos ++;
    }
  }
  CHECK_EQ(offset_pos, offset.size());
  CHECK_EQ(index_pos, index.size());
  CHECK_EQ(value_pos, value.size());

  // fill info
  MatrixPtrList<V> res;
  MatrixInfo label_info;
  string label_str = "type: DENSE row_major: true row { begin: 0 end: "
                     + std::to_string(info.num_instances())
                     + " } col { begin: 0 end: 1 } nnz: "
                     + std::to_string(info.num_instances())
                     + " sizeof_value: " + std::to_string(sizeof(V));
  google::protobuf::TextFormat::ParseFromString(label_str, &label_info);
  res.push_back(MatrixPtr<V>(new DenseMatrix<V>(label_info, label)));

  MatrixInfo f;
  if (info.feature_type() == FeatureGroupInfo::DENSE)
    f.set_type(MatrixInfo::DENSE);
  else if (info.feature_type() == FeatureGroupInfo::SPARSE)
    f.set_type(MatrixInfo::SPARSE);
  else if (info.feature_type() == FeatureGroupInfo::SPARSE_BINARY)
    f.set_type(MatrixInfo::SPARSE_BINARY);
  f.set_row_major(true);
  f.mutable_row()->set_begin(0);
  f.mutable_row()->set_end(info.num_instances());
  f.mutable_col()->set_begin(info.feature_begin());
  f.mutable_col()->set_end(info.feature_end());
  f.set_nnz(info.num_entries());
  f.set_sizeof_index(sizeof(uint64));
  f.set_sizeof_value(sizeof(V));
  res.push_back(MatrixPtr<V>(new SparseMatrix<uint64, V>(f, offset, index, value)));

  return res;
}


template<typename V>
MatrixPtr<V> readMatrixFromBin(const std::string& file) {
  return readMatrixFromBin<V>(SizeR::all(), file);
}

template<typename V>
MatrixPtr<V> readMatrixFromBin(SizeR outer_range, const std::string& file) {
  MatrixInfo info;
  ReadFileToProtoOrDie(file+".info", &info);
  if (outer_range == SizeR::all()) {
    if (info.row_major())
      outer_range.from(info.row());
    else
      outer_range.from(info.col());
  }
  CHECK(!outer_range.empty());
  CHECK_EQ(sizeof(V), info.sizeof_value());

  if (info.row_major())
    outer_range.to(info.mutable_row());
  else
    outer_range.to(info.mutable_col());

  if (info.type() == MatrixInfo::DENSE) {
    // read value
    size_t inner_size =
        info.row_major() ? SizeR(info.col()).size() : SizeR(info.row()).size();
    SizeR range = outer_range*inner_size;
    SArray<V> value;
    CHECK(value.readFromFile(range, file+".value"));
    info.set_nnz(range.size());
    return MatrixPtr<V>(new DenseMatrix<V>(info, value));
  } else {
    // read offset
    SArray<size_t> offset;
    CHECK(offset.readFromFile(
        SizeR(outer_range.begin(), outer_range.end()+1), file+".offset"));

    SizeR range(offset.front(), offset.back());

    if (range.begin() != 0) for (auto& s : offset) s -= range.begin();

    // read index
    CHECK(info.has_sizeof_index());
    size_t index_s = info.sizeof_index();
    SArray<char> index;
    CHECK(index.readFromFile(range*index_s, file+".index"));

    // read value
    SArray<V> value;
    if (info.type() == MatrixInfo::SPARSE)
      CHECK(value.readFromFile(range, file+".value"));

    info.set_nnz(range.size());

    if (index_s == 4) {
      return MatrixPtr<V>(new SparseMatrix<uint32, V>(
          info, offset, SArray<uint32>(index), value));
    } else if (index_s == 8) {
      return MatrixPtr<V>(new SparseMatrix<uint64, V>(
          info, offset, SArray<uint64>(index), value));
    } else {
      CHECK(false) << "unknown type" << info.DebugString();
    }
  }
  return MatrixPtr<V>(nullptr);
}

template<typename V>
MatrixPtrList<V> readMatricesFromBin(
    SizeR outer_range, const std::vector<std::string>& files) {
  MatrixPtrList<V> res;
  for (auto& f : files) res.push_back(readMatrixFromBin<V>(outer_range, f));
  return res;
}

template<typename V>
MatrixPtrList<V> readMatrices(const DataConfig& config) {
  std::vector<std::string> files;
  for (int i = 0; i < config.files_size(); ++i) files.push_back(config.files(i));
  if (config.format() == DataConfig::BIN) {
    SizeR outer_range = SizeR::all();
    if (config.has_range()) outer_range.from(config.range());
    return readMatricesFromBin<V>(outer_range, files);
  } else if (config.format() == DataConfig::PROTO) {
    return readMatricesFromProto<V>(files);
  } else {
    CHECK(false) << "unknonw data format: " << config.DebugString();
  }
  return MatrixPtrList<V>();
}


} // namespace PS
