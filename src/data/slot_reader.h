#pragma once
#include "base/shared_array_inl.h"
#include "proto/example.pb.h"
#include "data/common.h"

namespace PS {

// read all slots in *data* with multithreadd, save them into *cache*.
class SlotReader {
 public:
  SlotReader() { }
  SlotReader(const DataConfig& data, const DataConfig& cache) {
    init(data, cache);
  }

  void init(const DataConfig& data, const DataConfig& cache);

  // first read, then save
  int read(ExampleInfo* info = nullptr);

  template<typename V> MatrixInfo info(int slot_id) const {
    return readMatrixInfo(info_, slot_id, sizeof(uint64), sizeof(V));
  }

  // load a slot from cache
  SArray<size_t> offset(int slot_id);
  SArray<uint64> index(int slot_id);
  template<typename V> SArray<V> value(int slot_id) const;

  void clear(int slot_id) {
    offset_cache_.erase(slot_id);
    index_cache_.erase(slot_id);
  }

 private:
  string cacheName(const DataConfig& data, int slot_id) const;
  size_t nnzEle(int slot_id) const;
  bool readOneFile(const DataConfig& data, int ith_file);
  string cache_;
  DataConfig data_;
  // bool dump_to_disk_;
  ExampleInfo info_;
  std::unordered_map<int, SlotInfo> slot_info_;
  std::mutex mu_;
  size_t loaded_file_count_;
  std::vector<uint32> num_ex_;
  std::unordered_map<int, SArray<size_t>> offset_cache_;
  std::unordered_map<int, SArray<uint64>> index_cache_;
};

template<typename V> SArray<V> SlotReader::value(int slot_id) const {
  SArray<V> val;
  if (nnzEle(slot_id) == 0) return val;
  for (int i = 0; i < data_.file_size(); ++i) {
    string file = cacheName(ithFile(data_, i), slot_id) + ".value";
    SArray<char> comp; CHECK(comp.readFromFile(file));
    SArray<float> uncomp; uncomp.uncompressFrom(comp);
    size_t n = val.size();
    val.resize(n+uncomp.size());
    for (size_t i = 0; i < uncomp.size(); ++i) val[n+i] = uncomp[i];
  }
  CHECK_EQ(val.size(), nnzEle(slot_id)) << slot_id;
  return val;
}

} // namespace PS
