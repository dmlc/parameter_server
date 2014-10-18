#pragma once
#include "data/common.h"
#include "base/shared_array_inl.h"
#include "proto/example.pb.h"
#include "proto/matrix.pb.h"
#include "data/example_parser.h"
#include "util/filelinereader.h"
#include "base/matrix_io_inl.h"
namespace PS {

template<typename V>
class StreamReader {
 public:
  StreamReader() { }
  StreamReader(const DataConfig& data) { init(data); }
  void init(const DataConfig& data);

  // return false if error happens or reach the end of files. return true otherwise
  bool readMatrices(uint32 num_example, MatrixPtrList<V>* mat);
  bool readMatricesFromText(uint32 num_example, MatrixPtrList<V>* mat);

 private:
  bool readOneLineFromText(uint32 num_read);
  struct VSlot {
    SArray<V> val;
    SArray<uint64> col_idx;
    SArray<uint16> row_siz;
    bool empty() { return val.empty() && col_idx.empty() && row_siz.empty(); }
    void clear() { val.clear(); col_idx.clear(); row_siz.clear(); }
  };
  std::vector<VSlot> vslots_;
  ExampleParser parser_;
  DataConfig data_;
  int cur_file_ = 0;

  static const int kMaxLineLength_ = 60 * 1024;
  char line_[kMaxLineLength_];
  File* data_file_ = nullptr;
  bool reach_data_end_ = false;
};

template<typename V>
void StreamReader<V>::init(const DataConfig& data) {
  data_ = data;
  parser_.init(data_.text(), data_.ignore_feature_group());
  vslots_.resize(parser_.maxSlotID());
  // line_ = new char[kMaxLineLength_];
}

template<typename V>
bool StreamReader<V>::readOneLineFromText(uint32 num_read) {
  char* result;
  while (true) {
    // open the file if necessary
    if (data_file_ == nullptr) {
      if (cur_file_ >= data_.file_size()) {
        reach_data_end_ = true;
        return false;
      }
      data_file_ = File::open(ithFile(data_, cur_file_), "r");
      if (data_file_ == nullptr) return false;
    }
    // read a line
    result = data_file_->readLine(line_, kMaxLineLength_);
    if (result != nullptr) break;
    // end of the file, try to open a new one
    data_file_->close();
    data_file_ = nullptr;
    ++ cur_file_;
  }

  // parse
  Example ex; if (!parser_.toProto(result, &ex)) return false;

  // store them in slots
  for (int i = 0; i < ex.slot_size(); ++i) {
    const auto& slot = ex.slot(i);
    CHECK_LT(slot.id(), kSlotIDmax);
    auto& vslot = vslots_[slot.id()];
    int key_size = slot.key_size();
    for (int j = 0; j < key_size; ++j) vslot.col_idx.pushBack(slot.key(j));
    int val_size = slot.val_size();
    for (int j = 0; j < val_size; ++j) vslot.val.pushBack(slot.val(j));
    while (vslot.row_siz.size() < num_read) vslot.row_siz.pushBack(0);
    vslot.row_siz.pushBack(std::max(key_size, val_size));
  }

  return true;
}


template<typename V>
bool StreamReader<V>::readMatricesFromText(uint32 num_example, MatrixPtrList<V>* mat) {
  uint32 num_read = 0;
  while (num_read < num_example && readOneLineFromText(num_read)) { ++ num_read; }

  auto info = parser_.info();

  for (int i = 0; i < vslots_.size(); ++i) {
    if (vslots_[i].empty()) continue;
    MatrixInfo mat_info = readMatrixInfo(info, i, sizeof(uint64), sizeof(V));
    if (mat_info.type() == MatrixInfo::DENSE) {
      mat->push_back(MatrixPtr<V>(new DenseMatrix<V>(mat_info, vslots_[i].val)));
      CHECK_EQ(vslots_[i].val.size(), num_read);
    } else {
      auto& rs = vslots_[i].row_siz;
      size_t n = rs.size();
      SArray<size_t> offset(n + 1); offset[0] = 0;
      for (size_t j = 0; j < n; ++j) offset[j+1] = offset[j] + rs[j];
      mat->push_back(MatrixPtr<V>(new SparseMatrix<uint64, V>(
          mat_info, offset, vslots_[i].col_idx, vslots_[i].val)));
      CHECK_EQ(n, num_read);
      CHECK_EQ(vslots_[i].col_idx.size(), offset.back());
      if (!vslots_[i].val.empty()) {
        CHECK_EQ(vslots_[i].val.size(), offset.back());
      }
    }
    vslots_[i].clear();
  }
  parser_.clear();
  return !reach_data_end_;
}


template<typename V>
bool StreamReader<V>::readMatrices(uint32 num_example, MatrixPtrList<V>* mat) {
  mat->clear();
  switch(data_.format()) {
    case DataConfig::TEXT:
      return readMatricesFromText(num_example, mat);
      // case DataConfig::PROTO:
      //   return readMatricesFromProto(data, mat);
      // case DataConfig::BIN:
      //   return readMatricesFromBin(data, mat);
    default:
      LL << "unknonw data format: " << data_.DebugString();

  }
  return false;
}


} // namespace PS
