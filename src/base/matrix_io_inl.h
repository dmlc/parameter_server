#pragma once
#include "base/matrix_io.h"
#include "util/filelinereader.h"
// #include "data/text_parser.h"
#include "util/file.h"
#include "data/common.h"
namespace PS {

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
  // FIXME
  CHECK_LT(i, info.fea_grp_size());
  auto g = info.fea_grp(i);
  f.set_row_major(true);
  f.set_id(g.grp_id());
  f.mutable_row()->set_begin(0);
  f.mutable_row()->set_end(info.num_ins());
  f.mutable_col()->set_begin(info.fea_begin());
  f.mutable_col()->set_end(info.fea_end());
  // f.mutable_col()->set_begin(g.fea_begin());
  // f.mutable_col()->set_end(g.fea_end());
  f.set_nnz(g.nnz_ele());
  f.set_sizeof_index(sizeof(uint64));
  f.set_sizeof_value(sizeof(V));
  // f.set_nnz_per_row((double) g.nnz_ele() / (double) g.nnz_ins());
  *f.mutable_ins_info() = info;
  return f;
}

// template<typename V>
// void addLabel(const InstanceInfo& info, SArray<V> label, MatrixPtrList<V>* mat) {
//   MatrixInfo label_info;
//   string label_str =
//       "type: DENSE row_major: true row { begin: 0 end: "
//       + std::to_string(info.num_ins()) + " } col { begin: 0 end: 1 } nnz: "
//       + std::to_string(info.num_ins()) + " sizeof_value: "
//       + std::to_string(sizeof(V));
//   google::protobuf::TextFormat::ParseFromString(label_str, &label_info);
//   *label_info.mutable_ins_info() = info;
//   mat->push_back(MatrixPtr<V>(new DenseMatrix<V>(label_info, label)));
// }

// // template<typename V>
// // void createMatrices(const InstanceInfo& info, SArray<size_t> offset,
// //                     SArray<uint64> index, SArray<V> value, ) {
// //   // the feature matrix
// //   MatrixInfo f = readMatrixInfo<V>(info, 0);
// //   mat->push_back(MatrixPtr<V>(new SparseMatrix<uint64, V>(f, offset, index, value)));
// // }

// // label, feature_group 1, feature_group 2, ...
// // TODO do not support dense feature group yet...
// template<typename V>
// bool readMatricesFromProto(const DataConfig& data, MatrixPtrList<V>* mat) {
// //   // load info
// //   InstanceInfo info;
// //   for (int i = 0; i < data.file_size(); ++i) {
// //     auto f = ithFile(data, i, ".info");
// //     InstanceInfo tmp;
// //     if (!readFileToProto(f, &tmp)) {
// //       LL << "failed to load instance info from " << f.DebugString();
// //       return false;
// //     }
// //     info = mergeInstanceInfo(info, tmp);
// //   }
// //   if (info.fea_group_size() <= 1) {
// //     LL << "error in info:\n" << info.DebugString();
// //     return false;
// //   }

// //   // allocate data
// //   SArray<V> label(info.num_ins());
// //   SArray<size_t> offset(info.num_ins() + 1);
// //   offset[0] = 0;
// //   SArray<uint64> index(info.fea_group(0).nnz());
// //   SArray<V> value;
// //   bool binary = info.fea_type() == InstanceInfo::SPARSE_BINARY;
// //   if (!binary) value.resize(info.fea_group(0).nnz());

// //   // file data
// //   uint64 offset_pos = 0, index_pos = 0, value_pos = 0, label_pos = 0;
// //   Instance record;
// //   for (int i = 0; i < data.file_size(); ++i) {
// //     auto f = ithFile(data, i, ".recordio");
// //     File *in = File::open(f, "r");
// //     if (in == NULL || !in->open()) return false;
// //     RecordReader r(in);
// //     while (r.ReadProtocolMessage(&record)) {
// //       label[label_pos++] = record.label();
// //       int n = record.fea_id_size();
// //       if (!binary) CHECK_EQ(n, record.fea_val_size());
// //       for (int i = 0; i < n; ++i) {
// //         index[index_pos++] = record.fea_id(i);
// //         if (!binary) value[value_pos++] = record.fea_val(i);
// //       }
// //       offset[offset_pos+1] = offset[offset_pos] + n;
// //       offset_pos ++;
// //     }
// //     in->close();
// //   }
// //   CHECK_EQ(offset_pos+1, offset.size());
// //   CHECK_EQ(index_pos, index.size());
// //   CHECK_EQ(value_pos, value.size());

// //   createMatrices(info, label, offset, index, value, mat);
//   return false;
// }

// template<typename V>
// bool readMatricesFromBin(const DataConfig& data, MatrixPtrList<V>* mat) {
//   // load matrices one by one
//   for (int i = 0; i < data.file_size(); ++i) {
//     // load info
//     MatrixInfo info;
//     auto f = ithFile(data, i, ".info");
//     if (!readFileToProto(f, &info)) {
//       // DD << "failed to load instance info from " << f.DebugString();
//       return false;
//     }
//     CHECK_EQ(sizeof(V), info.sizeof_value());

//     // the part of matrix to read
//     SizeR outer_range;
//     if (data.has_range()) {
//       outer_range = SizeR(data.range());
//     } else if (info.row_major()) {
//       outer_range.copyFrom(info.row());
//     } else {
//       outer_range.copyFrom(info.col());
//     }
//     CHECK(!outer_range.empty());

//     if (info.type() == MatrixInfo::DENSE) {
//       // read value
//       size_t inner_size =
//           info.row_major() ? SizeR(info.col()).size() : SizeR(info.row()).size();
//       SizeR data_range = outer_range*inner_size;
//       SArray<V> value;
//       auto f = ithFile(data, i, ".value");
//       if(!value.readFromFile(data_range, f)) {
//         // DD << "failed to read value: " << f.DebugString();
//         return false;
//       }
//       info.set_nnz(data_range.size());
//       mat->push_back(MatrixPtr<V>(new DenseMatrix<V>(info, value)));
//     } else {
//       // read offset
//       SArray<size_t> offset;
//       auto f1 = ithFile(data, i, ".offset");

//       if (!offset.readFromFile(
//               SizeR(outer_range.begin(), outer_range.end()+1), f1)) {
//         // DD << "failed to read offset: " << f1.DebugString();
//         return false;
//       }
//       SizeR data_range(offset.front(), offset.back());
//       if (data_range.begin() != 0) for (auto& s : offset) s -= data_range.begin();

//       // read index
//       CHECK(info.has_sizeof_index());
//       size_t index_s = info.sizeof_index();
//       SArray<char> index;
//       auto f2 = ithFile(data, i, ".index");
//       if (!index.readFromFile(data_range*index_s, f2)) {
//         // DD << "failed to read index: " << f2.DebugString();
//         return false;
//       }

//       // read value
//       SArray<V> value;
//       if (info.type() == MatrixInfo::SPARSE) {
//         auto f3 = ithFile(data, i, ".value");
//         if (!value.readFromFile(data_range, f3)) {
//           // DD << "failed to read value: " << f3.DebugString();
//           return false;
//         }
//       }

//       info.set_nnz(data_range.size());
//       if (index_s == 4) {
//         mat->push_back(MatrixPtr<V>(new SparseMatrix<uint32, V>(
//             info, offset, SArray<uint32>(index), value)));
//       } else if (index_s == 8) {
//         mat->push_back(MatrixPtr<V>(new SparseMatrix<uint64, V>(
//             info, offset, SArray<uint64>(index), value)));
//       } else {
//         // DD << "unknown index type" << info.DebugString();
//         return false;
//       }
//     }
//   }
//   return true;
// }

// template<typename V>
// bool readMatricesFromText(const DataConfig& data, MatrixPtrList<V>* mat) {
//   // TODO. multi-thread
//   TextParser parser(data.text(), data.ignore_feature_group());

//   SArray<V> label;
//   struct Slot {
//     SArray<V> val;
//     SArray<uint64> col_idx;
//     SArray<uint32> row_idx;
//     SArray<uint16> row_siz;
//   };
//   Slot slots[kGrpIDmax];
//   uint32 num_ins = 0;

//   std::function<void(char*)> handle = [&] (char *line) {
//     // parse one text line
//     Instance ins; if (!parser.toProto(line, &ins)) return;
//     // store them
//     label.pushBack(ins.label());
//     for (int i = 0; i < ins.fea_grp_size(); ++i) {
//       const auto& grp = ins.fea_grp(i);
//       CHECK_LT(grp.grp_id(), kGrpIDmax);
//       auto& slot = slots[grp.grp_id()];
//       int fea_size = grp.fea_id_size();
//       for (int j = 0; j < fea_size; ++j) {
//         slot.col_idx.pushBack(grp.fea_id(j));
//         if (grp.fea_val_size() == fea_size) slot.val.pushBack(grp.fea_val(j));
//       }
//       slot.row_idx.pushBack(num_ins);
//       slot.row_siz.pushBack(fea_size);
//     }
//     ++ num_ins;
//   };

//   for (int i = 0; i < data.file_size(); ++i) {
//     // construct a DataConfig containing only one file
//     DataConfig current_data_conf = data;
//     current_data_conf.clear_file();
//     current_data_conf.add_file(data.file(i));

//     // parse file
//     FileLineReader reader(current_data_conf);
//     reader.set_line_callback(handle);
//     reader.Reload();
//   }
//   auto info = parser.info();

//   // construct label
//   addLabel(info, label, mat);

//   int grp_num = 0;
//   for (int i = 0; i < kGrpIDmax; ++i) {
//     // align index
//     auto& slot = slots[i];
//     uint32 n = (uint32)slot.row_idx.size();
//     if (n == 0) continue;
//     SArray<size_t> offset(num_ins+1); offset[0] = 0;
//     uint32 k = 0;
//     size_t t = 0;
//     for (uint32 j = 0; j < num_ins; ++j) {
//       if (k >= n || j < slot.row_idx[k]) {
//         offset[j+1] = offset[j];
//       } else {
//         offset[j+1] = offset[j] + slot.row_siz[k];
//         t += slot.row_siz[k];
//         ++ k;
//       }
//     }
//     CHECK_EQ(t, slot.col_idx.size());
//     CHECK_EQ(offset.back(), slot.col_idx.size());
//     // construct the fea group matrix
//     MatrixInfo f = readMatrixInfo<V>(info, grp_num);
//     mat->push_back(MatrixPtr<V>(
//         new SparseMatrix<uint64, V>(f, offset, slot.col_idx, slot.val)));
//     ++ grp_num;
//     slot.row_idx.clear();
//     slot.row_siz.clear();
//   }
//   return true;
// }

// template<typename V> bool
// readMatrices(const DataConfig& data, MatrixPtrList<V>* mat) {
//   mat->clear();
//   switch(data.format()) {
//     case DataConfig::PROTO:
//       return readMatricesFromProto<V>(data, mat);
//     case DataConfig::TEXT:
//       return readMatricesFromText<V>(data, mat);
//     case DataConfig::BIN:
//       return readMatricesFromBin<V>(data, mat);
//     default: {
//       LL << "unknonw data format: " << data.DebugString();
//     }
//   }
//   return false;
// }

// template<typename V>
// MatrixPtrList<V> readMatricesOrDie(const DataConfig& data) {
//   MatrixPtrList<V> mat;
//   CHECK(readMatrices(data, &mat));
//   return mat;
// }

} // namespace PS
