#include "system/workload.h"

namespace PS {

bool Workload::GetCache(KeyRange range, cksum_t cksum, RawArray* keys) const {
  auto it = cached_keylist_.find(range);
  if (it == cached_keylist_.end())
    return false;
  if (cksum != 0 && it->second.cksum() != cksum)
    return false;
  if (keys)
    *keys = it->second;
  return true;
}

} // namespace PS
