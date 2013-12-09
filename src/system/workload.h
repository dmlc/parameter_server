#pragma once
#include "util/rawarray.h"
#include "util/range.h"
#include "util/key.h"
#include "system/node.h"

namespace PS {

typedef Range<size_t> DataRange;

// the workload associated to the node.
class Workload {
 public:
  Workload() { }
  ~Workload() { }
  Workload(uid_t id, KeyRange key) : node_(id), key_range_(key) { }
  Workload(uid_t id, KeyRange key, DataRange data) :
      node_(id), key_range_(key), data_range_(data) { }

  uid_t node() const { return node_; }
  // the key range this node maintains, usually for the server
  KeyRange key_range() const { return key_range_; }
  // the data ramge this node,maintains, usually for client nodes. the range is
  // [0,1], since probably we don't know how many samples there are
  DataRange data_range() const { return data_range_; }

  // return false if the key lists specifiyed by the range and signiture is not
  // cached, otherwise, return true and return the cached key lists
  // cksum = 0: ignore the checksum
  bool GetCache(KeyRange range, cksum_t cksum = 0, RawArray* keys = NULL) const;
  void SetCache(KeyRange range, cksum_t cksum, const RawArray& keys) {
    // just double check
    CHECK_EQ(cksum, keys.cksum());
    cached_keylist_[range] = keys;
  }
 private:
  uid_t node_;
  KeyRange key_range_;
  DataRange data_range_;
  map<KeyRange, RawArray> cached_keylist_;
};

} // namespace PS
