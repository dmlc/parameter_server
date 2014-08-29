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

////////// handle info //////////
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
  C.set_num_ins(A.num_ins() + B.num_ins());
  C.clear_fea_group();
  for (int i = 0; i < as; ++i) {
    *C.add_fea_group() = mergeFeatureGroupInfo(A.fea_group(i), B.fea_group(i));
  }
  return C;
}

static InstanceInfo readInstanceInfo(const std::vector<std::string>& files) {
  InstanceInfo info, tmp;
  for (const auto& f : files) {
    ReadFileToProtoOrDie(f, &tmp);
    info = mergeInstanceInfo(info, tmp);
  }
  return info;
}

static InstanceInfo readInstanceInfo(const DataConfig& config) {
  CHECK_EQ(config.format(), DataConfig::PROTO);
  std::vector<std::string> files;
  for (int i = 0; i < config.file_size(); ++i) {
    files.push_back(config.file(i));
  }
  return readInstanceInfo(files);
}

template<typename V>
MatrixInfo readMatrixInfo(const InstanceInfo& info, int i) {
  MatrixInfo f;
  if (info.fea_type() == InstanceInfo::DENSE) {
    f.set_type(MatrixInfo::DENSE);
  } else if (info.fea_type() == InstanceInfo::SPARSE) {
    f.set_type(MatrixInfo::SPARSE);
  } else if (info.fea_type() == InstanceInfo::SPARSE_BINARY) {
    f.set_type(MatrixInfo::SPARSE_BINARY);
  }
  auto g = info.fea_group(i);
  f.set_row_major(true);
  f.set_id(g.group_id());
  f.mutable_row()->set_begin(0);
  f.mutable_row()->set_end(info.num_ins());
  f.mutable_col()->set_begin(g.fea_begin());
  f.mutable_col()->set_end(g.fea_end());
  f.set_nnz(g.nnz());
  f.set_sizeof_index(sizeof(uint64));
  f.set_sizeof_value(sizeof(V));
  f.set_nnz_per_row((double) g.nnz() / (double) info.num_ins());
  return f;
}

// label, feature_group 1, feature_group 2, ...
// TODO do not support dense feature group yet...
template<typename V>
MatrixPtrList<V> readMatricesFromProto(const DataConfig& data) {
  // load info
  std::vector<RecordReader> readers;
  InstanceInfo info;
  for (int i = 0; i < data.file_size(); ++i) {
    auto f = data; f.clear_file();
    f.add_file(data.file(i));
    File *in = File::openOrDie(f, "r");

    RecordReader r(in);
    InstanceInfo ins;
    CHECK(r.ReadProtocolMessage(&ins));
    info = mergeInstanceInfo(info, ins);
    readers.push_back(r);
  }
  // LL << info.DebugString();

  // allocate data
  SArray<V> label(info.num_ins());
  SArray<size_t> offset(info.num_ins()+1);
  offset[0] = 0;
  CHECK_GT(info.fea_group_size(), 1);
  SArray<uint64> index(info.fea_group(0).nnz());
  SArray<V> value;
  bool binary = info.fea_type() == InstanceInfo::SPARSE_BINARY;
  if (!binary) value.resize(info.fea_group(0).nnz());

  // file data
  uint64 offset_pos = 0, index_pos = 0, value_pos = 0, label_pos = 0;
  Instance record;
  for (auto& r : readers) {
    while (r.ReadProtocolMessage(&record)) {
      label[label_pos++] = record.label();
      int n = record.fea_id_size();
      for (int i = 0; i < n; ++i) {
        index[index_pos++] = record.fea_id(i);
        if (!binary) value[value_pos++] = record.fea_val(i);
      }
      offset[offset_pos+1] = offset[offset_pos] + n;
      offset_pos ++;
    }
  }
  CHECK_EQ(offset_pos+1, offset.size());
  CHECK_EQ(index_pos, index.size());
  CHECK_EQ(value_pos, value.size());

  // construct the matrices
  MatrixPtrList<V> res;
  MatrixInfo label_info;
  string label_str = "type: DENSE row_major: true row { begin: 0 end: "
                     + std::to_string(info.num_ins())
                     + " } col { begin: 0 end: 1 } nnz: "
                     + std::to_string(info.num_ins())
                     + " sizeof_value: " + std::to_string(sizeof(V));
  google::protobuf::TextFormat::ParseFromString(label_str, &label_info);
  res.push_back(MatrixPtr<V>(new DenseMatrix<V>(label_info, label)));

  MatrixInfo f = readMatrixInfo<V>(info, 0);
  for (int i = 1; i < info.fea_group_size(); ++i) {
    *f.add_group_info() = info.fea_group(i);
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
    return readMatricesFromProto<V>(config);
  } else {
    CHECK(false) << "unknonw data format: " << config.DebugString();
  }
  return MatrixPtrList<V>();
}


} // namespace PS
