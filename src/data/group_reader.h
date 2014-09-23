#pragma once
#include "base/shared_array_inl.h"
#include "proto/instance.pb.h"
#include "data/common.h"

namespace PS {

class GroupReader {
 public:
  GroupReader(const DataConfig& data, const DataConfig& cache);

  int read(InstanceInfo* info = nullptr);
  SArray<uint64> index(int grp_id);
  SArray<size_t> offset(int grp_id);
  SArray<float> value(int grp_id);
 private:
  string cacheName(const DataConfig& data, int grp_id) {
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

} // namespace PS
