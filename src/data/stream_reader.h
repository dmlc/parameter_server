#pragma once
#include "data/common.h"
#include "base/shared_array_inl.h"
#include "proto/example.pb.h"
#include "proto/matrix.pb.h"
#include "data/example_parser.h"
#include "util/filelinereader.h"
#include "base/matrix_io_inl.h"
#include "util/recordio.h"
namespace PS {

template<typename V>
class StreamReader {
 public:
  StreamReader() { }
  StreamReader(const DataConfig& data) { init(data); }
  void init(const DataConfig& data);

  // return false if error happens or reach the end of files. return true otherwise
  bool readMatrices(
      uint32 num_example,
      MatrixPtrList<V>* matrices,
      std::vector<Example>* examples = nullptr);

 private:
  bool readMatricesFromText(uint32 num_example, MatrixPtrList<V>* matrices);
  bool readMatricesFromProto(uint32 num_example, MatrixPtrList<V>* matrices);
  void parseExample(const Example& ex, int num_read);
  void fillMatrices(MatrixPtrList<V>* mat);

  // return true if opened success, false if done
  bool openNextFile();
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
  int next_file_ = 0;
  int max_num_files_ = 0;

  static const int kMaxLineLength_ = 60 * 1024;
  char line_[kMaxLineLength_];
  File* data_file_ = nullptr;
  bool reach_data_end_ = false;
  std::vector<Example>* examples_ = nullptr;
};

template<typename V>
bool StreamReader<V>::openNextFile() {
  if (data_file_) { data_file_->close(); data_file_ = nullptr; }
  while (true) {
    if (next_file_ >= max_num_files_) {
      reach_data_end_ = true;
      return false;
    }
    data_file_ = File::open(ithFile(data_, next_file_ ++), "r");
    if (data_file_ != nullptr) break;
  }
  return true;
}

template<typename V>
void StreamReader<V>::init(const DataConfig& data) {
  data_ = data;
  parser_.init(data_.text(), data_.ignore_feature_group());
  vslots_.resize(parser_.maxSlotID());
  max_num_files_ = data_.file_size();
  if (data_.max_num_files_per_worker() >= 0) {
    max_num_files_ = std::min(max_num_files_, data_.max_num_files_per_worker());
  }
  openNextFile();
}


template<typename V>
void StreamReader<V>::parseExample(const Example& ex, int num_read) {
  if (examples_) examples_->push_back(ex);
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
}

template<typename V>
void StreamReader<V>::fillMatrices(MatrixPtrList<V>* mat) {
  auto info = parser_.info();
  for (int i = 0; i < vslots_.size(); ++i) {
    if (vslots_[i].empty()) continue;
    MatrixInfo mat_info = readMatrixInfo(info, i, sizeof(uint64), sizeof(V));
    if (mat_info.type() == MatrixInfo::DENSE) {
      mat->push_back(MatrixPtr<V>(new DenseMatrix<V>(mat_info, vslots_[i].val)));
      // CHECK_EQ(vslots_[i].val.size(), num_read);
    } else {
      auto& rs = vslots_[i].row_siz;
      size_t n = rs.size();
      SArray<size_t> offset(n + 1); offset[0] = 0;
      for (size_t j = 0; j < n; ++j) offset[j+1] = offset[j] + rs[j];
      mat->push_back(MatrixPtr<V>(new SparseMatrix<uint64, V>(
          mat_info, offset, vslots_[i].col_idx, vslots_[i].val)));
      // CHECK_EQ(n, num_read);
      CHECK_EQ(vslots_[i].col_idx.size(), offset.back());
      if (!vslots_[i].val.empty()) {
        CHECK_EQ(vslots_[i].val.size(), offset.back());
      }
    }
    vslots_[i].clear();
  }
  parser_.clear();
}

template<typename V>
bool StreamReader<V>::readMatricesFromProto(uint32 num_ex, MatrixPtrList<V>* mat) {
  uint32 num_read = 0;
  RecordReader reader(data_file_);
  Example ex;
  while (num_read < num_ex && !reach_data_end_) {
    while (true) {
      // read a record
      if (reader.ReadProtocolMessage(&ex)) {
        parseExample(ex, num_read);
        ++ num_read;
        break;
      } else {
        if (!openNextFile()) break;
        reader = RecordReader(data_file_);
      }
    }
  }
  fillMatrices(mat);
  return !reach_data_end_;
}

template<typename V>
bool StreamReader<V>::readMatricesFromText(uint32 num_ex, MatrixPtrList<V>* mat) {
  uint32 num_read = 0;
  while (num_read < num_ex && !reach_data_end_) {
    while (true) {
      // read a line
      char* result = data_file_->readLine(line_, kMaxLineLength_);
      if (result != nullptr) {
        Example ex;
        if (!parser_.toProto(result, &ex)) continue;
        parseExample(ex, num_read);
        ++ num_read;
        break;
      } else {
        if (!openNextFile()) break;
      }
    }
  }
  fillMatrices(mat);
  return !reach_data_end_;
}

template<typename V>
bool StreamReader<V>::readMatrices(
    uint32 num_example, MatrixPtrList<V>* matrices, std::vector<Example>* examples) {
  examples_ = examples;
  if (examples_) examples_->clear();
  matrices->clear();
  switch(data_.format()) {
    case DataConfig::TEXT:
      return readMatricesFromText(num_example, matrices);
    case DataConfig::PROTO:
      return readMatricesFromProto(num_example, matrices);
      // case DataConfig::BIN:
      //   return readMatricesFromBin(data, mat);
    default:
      LL << "unknonw data format: " << data_.DebugString();

  }
  return false;
}


} // namespace PS
