#pragma once
#include "base/shared_array_inl.h"
#include "proto/instance.pb.h"
#include "data/common.h"

namespace PS {

// read groups in *data* with multithreadd, save them into *cache*.
class GroupReader {
 public:
  void init(const DataConfig& data, const DataConfig& cache);

  // first read, then save
  int read(InstanceInfo* info = nullptr);

  // load group from cache
  MatrixInfo info(int grp_id) const;

  SArray<size_t> offset(int grp_id) const;
  SArray<uint64> index(int grp_id) const;
  template<typename V> SArray<V> value(int grp_id) const;

 private:
  string cacheName(const DataConfig& data, int grp_id) const {
    CHECK_GT(data.file_size(), 0);
    return cache_ + getFilename(data.file(0)) + "_grp_" + std::to_string(grp_id);
  }
  bool readOneFile(const DataConfig& data);
  string cache_;
  DataConfig data_;
  bool dump_to_disk_;
  InstanceInfo info_;
  std::unordered_map<int, FeatureGroupInfo> fea_grp_;
  std::mutex mu_;
};

template<typename V> SArray<V> GroupReader::value(int grp_id) const {
  SArray<V> val;
  auto it = fea_grp_.find(grp_id);
  if (it == fea_grp_.end()) return val;
  for (int i = 0; i < data_.file_size(); ++i) {
    string file = cacheName(ithFile(data_, i), grp_id) + ".value";
    SArray<char> comp; CHECK(comp.readFromFile(file));
    SArray<float> uncomp; uncomp.uncompressFrom(comp);
    size_t n = val.size();
    val.resize(n+uncomp.size());
    for (size_t i = 0; i < uncomp.size(); ++i) val[n+i] = uncomp[i];
  }
  CHECK_EQ(val.size(), it->second.nnz_ele());
  return val;
}

} // namespace PS
