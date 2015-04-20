#include "system/assigner.h"
#include "data/common.h"
namespace PS {

void DataAssigner::set(const DataConfig& data, int num, bool local) {
  // search all files
  CHECK_GT(num, 0);
  parts_.resize(num);
  if (local) {
    for (int i = 0; i < num; ++i) parts_[i].CopyFrom(data);
    return;
  }

  CHECK_GT(data.replica(), 0);
  DataConfig files = searchFiles(data);
  VLOG(1) << "find " << files.file_size()
          << " files: " << files.ShortDebugString();

  // divide them
  for (int r = 0; r < data.replica(); ++r) {
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
  VLOG(1) << "divide into " << num  << " jobs";
}

bool DataAssigner::next(DataConfig *data) {
  if (cur_i_ >= parts_.size()) return false;
  *data = parts_[cur_i_ ++];
  return true;
}

} // namespace PS
