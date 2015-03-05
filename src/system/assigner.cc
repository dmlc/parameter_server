#include "system/assigner.h"
#include "data/common.h"
namespace PS {

void DataAssigner::set(const DataConfig& data, int num) {
  // search all files
  CHECK_GT(num, 0);
  CHECK_GT(data.duplication(), 0);
  DataConfig files = searchFiles(data);
  VLOG(1) << "find " << files.file_size() << "files: " << files.ShortDebugString();

  // divide them
  parts_.resize(num);
  for (int r = 0; r < data.duplication(); ++r) {
    if (data.shuffle()) files = shuffleFiles(files);
    auto pts = divideFiles(files, num);
    if (r == 0) {
      parts_ = pts;
    } else {
      for (int i = 0; i < num; ++i) {
        parts_[i] = appendFiles(parts_[i], pts[i]);
      }
    }
  }
}

bool DataAssigner::next(DataConfig *data) {
  *data = parts_[cur_i ++];
  return cur_i == parts_.size();
}

} // namespace PS
