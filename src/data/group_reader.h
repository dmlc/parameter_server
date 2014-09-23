#pragma once
#include "base/shared_array_inl.h"
#include "data/common.h"

namespace PS {

class GroupReader {
 public:
  GroupReader(const DataConfig& cache);

  int read(const DataConfig& data, InstanceInfo* info = nullptr);
  SArray<uint64> index(int grp_id);
  SArray<size_t> offset(int grp_id);
  SArray<float> value(int grp_id);
 private:
  bool readOneFile(const DataConfig& data);
  string cache_;
  bool dump_to_disk_;
  InstanceInfo info_;
  std::mutex mu_;
};

} // namespace PS
