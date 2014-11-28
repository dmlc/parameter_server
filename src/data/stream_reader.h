#pragma once
#include "data/common.h"
#include "base/shared_array_inl.h"
#include "proto/example.pb.h"
#include "proto/matrix.pb.h"
#include "data/text_parser.h"
#include "data/info_parser.h"
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
      uint32 num_examples,
      MatrixPtrList<V>* matrices,
      std::vector<Example>* examples = nullptr);

  // return false if error happens or reach the end of files. return true otherwise
  bool readExamples(uint32 num_examples, std::vector<Example>* examples) {
    return readMatrices(num_examples, nullptr, examples);
  }
 private:
  bool readMatricesFromText();
  bool readMatricesFromProto();
  void parseExample(const Example& ex, int num_read);
  void fillMatrices();

  // return true if opened success, false if done
  bool openNextFile();
  struct VSlot {
    SArray<V> val;
    SArray<uint64> col_idx;
    SArray<uint16> row_siz;
    bool empty() { return val.empty() && col_idx.empty() && row_siz.empty(); }
    // void clear() { val.clear(); col_idx.clear(); row_siz.clear(); }
  };
  std::vector<VSlot> vslots_;
  ExampleParser text_parser_;
  InfoParser info_parser_;
  DataConfig data_;
  int next_file_ = 0;
  int max_num_files_ = 0;

  static const int kMaxLineLength_ = 60 * 1024;
  char line_[kMaxLineLength_];
  File* data_file_ = nullptr;
  bool reach_data_end_ = false;

  // return
  uint32 num_examples_ = 0;
  MatrixPtrList<V>* matrices_ = nullptr;
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
  if (data_.format() == DataConfig::TEXT) {
    text_parser_.init(data_.text(), data_.ignore_feature_group());
  }
  max_num_files_ = data_.file_size();
  if (data_.max_num_files_per_worker() >= 0) {
    max_num_files_ = std::min(max_num_files_, data_.max_num_files_per_worker());
  }
  openNextFile();
}


template<typename V>
void StreamReader<V>::parseExample(const Example& ex, int num_read) {
  if (examples_) examples_->push_back(ex);
  if (!matrices_) return;
  if (!info_parser_.add(ex)) return;
  // store them in slots
  for (int i = 0; i < ex.slot_size(); ++i) {
    const auto& slot = ex.slot(i);
    CHECK_LT(slot.id(), kSlotIDmax);
    auto& vslot = vslots_[slot.id()];
    int key_size = slot.key_size();
    for (int j = 0; j < key_size; ++j) vslot.col_idx.pushBack(slot.key(j));
    int val_size = slot.val_size();
    for (int j = 0; j < val_size; ++j) {
      vslot.val.pushBack(slot.val(j));
    }
    while (vslot.row_siz.size() < num_read) vslot.row_siz.pushBack(0);
    vslot.row_siz.pushBack(std::max(key_size, val_size));
  }
}

template<typename V>
void StreamReader<V>::fillMatrices() {
  if (!matrices_) return;

  auto info = info_parser_.info();
  info_parser_.clear();
  for (int i = 0; i < vslots_.size(); ++i) {
    if (vslots_[i].empty()) continue;
    MatrixInfo mat_info = readMatrixInfo(info, i, sizeof(uint64), sizeof(V));
    if (mat_info.type() == MatrixInfo::DENSE) {
      matrices_->push_back(MatrixPtr<V>(new DenseMatrix<V>(mat_info, vslots_[i].val)));
    } else {
      auto& rs = vslots_[i].row_siz;
      size_t n = rs.size();
      SArray<size_t> offset(n + 1); offset[0] = 0;
      for (size_t j = 0; j < n; ++j) offset[j+1] = offset[j] + rs[j];
      matrices_->push_back(MatrixPtr<V>(new SparseMatrix<uint64, V>(
          mat_info, offset, vslots_[i].col_idx, vslots_[i].val)));
      CHECK_EQ(vslots_[i].col_idx.size(), offset.back());
      if (!vslots_[i].val.empty()) {
        CHECK_EQ(vslots_[i].val.size(), offset.back());
      }
    }
  }
}

template<typename V>
bool StreamReader<V>::readMatricesFromProto() { //uint32 num_ex, MatrixPtrList<V>* mat) {
  uint32 num_read = 0;
  RecordReader reader(data_file_);
  Example ex;
  while (num_read < num_examples_ && !reach_data_end_) {
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
  fillMatrices();
  return !reach_data_end_;
}

template<typename V>
bool StreamReader<V>::readMatricesFromText() { // uint32 num_ex, MatrixPtrList<V>* mat) {
  uint32 num_read = 0;
  while (num_read < num_examples_ && !reach_data_end_) {
    while (true) {
      // read a line
      char* result = data_file_->readLine(line_, kMaxLineLength_);
      if (result != nullptr) {
        // Chop the last linefeed if present.
        int len = strlen(result);
        if (len > 0 && result[len - 1] == '\n') {  // Linefeed.
          result[--len] = '\0';
        }
        if (len > 0 && result[len - 1] == '\r') {  // Carriage return.
          result[--len] = '\0';
        }
        Example ex;
        if (!text_parser_.toProto(result, &ex)) continue;
        parseExample(ex, num_read);
        ++ num_read;
        break;
      } else {
        if (!openNextFile()) break;
      }
    }
  }
  fillMatrices();
  return !reach_data_end_;
}

template<typename V>
bool StreamReader<V>::readMatrices(
    uint32 num_examples, MatrixPtrList<V>* matrices, std::vector<Example>* examples) {
  num_examples_ = num_examples;
  examples_ = examples; if (examples_) examples_->clear();
  matrices_ = matrices; if (matrices_) matrices_->clear();
  vslots_.resize(data_.ignore_feature_group() ? 2 : kSlotIDmax);
  bool ret = false;
  if (data_.format() == DataConfig::TEXT) {
    ret = readMatricesFromText();
  } else if (data_.format() == DataConfig::PROTO) {
    ret = readMatricesFromProto();
  } else {
    LL << "unknonw data format: " << data_.DebugString();
  }
  vslots_.clear();
  return ret;
}


} // namespace PS
