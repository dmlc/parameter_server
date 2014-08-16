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

static FeatureGroupInfo
mergeFeatureGroupInfo(const FeatureGroupInfo& A, const FeatureGroupInfo& B) {
  auto C = A;
  C.set_fea_begin(std::min(A.fea_begin(), B.fea_begin()));
  C.set_fea_end(std::max(A.fea_end(), B.fea_end()));
  C.set_nnz(A.nnz() + B.nnz());
  return C;
}

static InstanceInfo
mergeInstanceInfo(const InstanceInfo& A, const InstanceInfo& B) {
  auto as = A.fea_group_size();
  auto bs = B.fea_group_size();
  if (!as) return B;
  if (!bs) return A;
  CHECK_EQ(as, bs);

  CHECK_EQ(A.label_type(), B.label_type());
  CHECK_EQ(A.fea_type(), B.fea_type());

  InstanceInfo C = A;
  C.clear_fea_group();
  for (int i = 0; i < as; ++i) {
    *C.add_fea_group() = mergeFeatureGroupInfo(A.fea_group(i), B.fea_group(i));
  }
  return C;
}

template<typename V>
MatrixInfo readMatrixInfo(const FeatureGroupInfo g) {
  MatrixInfo f;
  if (g.feature_type() == FeatureGroupInfo::DENSE)
    f.set_type(MatrixInfo::DENSE);
  else if (g.feature_type() == FeatureGroupInfo::SPARSE)
    f.set_type(MatrixInfo::SPARSE);
  else if (g.feature_type() == FeatureGroupInfo::SPARSE_BINARY)
    f.set_type(MatrixInfo::SPARSE_BINARY);
  f.set_row_major(true);
  f.set_id(g.group_id());
  f.mutable_row()->set_begin(0);
  f.mutable_row()->set_end(g.num_instances());
  f.mutable_col()->set_begin(g.feature_begin());
  f.mutable_col()->set_end(g.feature_end());
  f.set_nnz(g.num_entries());
  f.set_sizeof_index(sizeof(uint64));
  f.set_sizeof_value(sizeof(V));
  f.set_nnz_per_row((double) g.num_entries() / (double) g.num_instances());
  return f;
}

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
  auto& all = info.all_group();
  bool binary = all.feature_type() == FeatureGroupInfo::SPARSE_BINARY;
  SArray<V> label(all.num_instances());
  SArray<size_t> offset(all.num_instances()+1);
  offset[0] = 0;
  SArray<uint64> index(all.num_entries());
  SArray<V> value;
  if (!binary) value.resize(all.num_entries());

  uint64 offset_pos = 0, index_pos = 0, value_pos = 0, label_pos = 0;

  // file data
  for (auto f : files) {
    File* in = File::openOrDie(f+".recordio", "r");
    RecordReader reader(in);
    Instance record;
    while (reader.ReadProtocolMessage(&record)) {
      label[label_pos++] = record.label();
      int n = record.feature_id_size();
      for (int i = 0; i < n; ++i) {
        index[index_pos++] = record.feature_id(i);
        if (!binary) value[value_pos++] = record.value(i);
      }
      offset[offset_pos+1] = offset[offset_pos] + n;
      offset_pos ++;
    }
  }
  CHECK_EQ(offset_pos+1, offset.size());
  CHECK_EQ(index_pos, index.size());
  CHECK_EQ(value_pos, value.size());

  // fill info
  MatrixPtrList<V> res;
  MatrixInfo label_info;
  string label_str = "type: DENSE row_major: true row { begin: 0 end: "
                     + std::to_string(info.all_group().num_instances())
                     + " } col { begin: 0 end: 1 } nnz: "
                     + std::to_string(info.all_group().num_instances())
                     + " sizeof_value: " + std::to_string(sizeof(V));
  google::protobuf::TextFormat::ParseFromString(label_str, &label_info);
  res.push_back(MatrixPtr<V>(new DenseMatrix<V>(label_info, label)));

  MatrixInfo f = readMatrixInfo<V>(info.all_group());
  for (int i = 0; i < info.individual_groups_size(); ++i) {
    *f.add_group_info() = info.individual_groups(i);
  }
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
      outer_range.copyFrom(info.row());
    else
      outer_range.copyFrom(info.col());
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
  for (int i = 0; i < config.file_size(); ++i) files.push_back(config.file(i));
  if (config.format() == DataConfig::BIN) {
    SizeR outer_range = SizeR::all();
    if (config.has_range()) outer_range.copyFrom(config.range());
    return readMatricesFromBin<V>(outer_range, files);
  } else if (config.format() == DataConfig::PROTO) {
    return readMatricesFromProto<V>(files);
  } else {
    CHECK(false) << "unknonw data format: " << config.DebugString();
  }
  return MatrixPtrList<V>();
}


} // namespace PS
