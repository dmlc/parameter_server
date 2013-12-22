#pragma once

// #define DEBUG_VECTORS_

#include "util/common.h"
#include "util/xarray.h"
#include "box/container.h"
#include "util/eigen3.h"

namespace PS {

typedef Range<size_t> IndexRange;
template <typename V>
class Vectors : public Container {
 public:
  typedef Eigen::Matrix<V, Eigen::Dynamic, Eigen::Dynamic> EMat;
  typedef Eigen::Matrix<V, Eigen::Dynamic, 1> EVec;
  typedef std::initializer_list<int> IntList;

  Vectors(const string& name, size_t global_length, int num_vec = 1,
          const XArray<Key>& global_keys = XArray<Key>());

  // initial keys and values
  void Init(size_t global_length, const XArray<Key>& global_keys);
  // wait until this container is initialized
  virtual void WaitInited();

  Status Push(KeyRange key_range = KeyRange::All(),
              IntList  vec_list  = {1},
              bool     delta     = kDelta,
              uid_t    dest      = kServer);

  Status Pull(KeyRange key_range = KeyRange::All(),
              IntList  vec_list  = {1},
              bool     delta     = kValue,
              uid_t    dest      = kServer);

  Status PushPull(KeyRange key_range     = KeyRange::All(),
                  IntList  push_vec_list = {0},
                  bool     push_delta    = kDelta,
                  IntList  pull_vec_list = {0},
                  bool     pull_delta    = kValue,
                  uid_t    dest          = kServer);

  Status GetLocalData(Mail *mail);
  Status MergeRemoteData(const Mail& mail);

  // the vector length (local)
  size_t len() { return vec_len_; }

  // return the i-th vector
  EMat vecs() { return local_; }

  Eigen::Block<EMat, EMat::RowsAtCompileTime, 1> vec(int i) {
    return Eigen::Block<EMat, EMat::RowsAtCompileTime, 1>(local_, i);
  }

  // reset column i of local_ to 0, and also its synced_ one
  void reset_vec(int i) {
    local_.col(i) = EVec::Zero(vec_len_);
    synced_.col(loc2syn_map_[i]) = EVec::Zero(vec_len_);
  }
  Eigen::Block<EMat, EMat::RowsAtCompileTime, 1> Vec(int i) {
    return vec(i);
  }

  string DebugString() {
    std::stringstream ss;
    ss << "local: " << std::endl << local_ << std::endl
       << "synced: " << std::endl << synced_;
    return ss.str();
  }
 private:
  // mapping the key range into local indeces
  IndexRange FindIndex(KeyRange kr, KeyPtrRange* key_ptr = NULL);
  size_t vec_len_;
  int num_vec_;
  // local working sets, it is a vec_len_ x num_vec_ matrix
  EMat local_;

  EMat synced_;
  XArray<Key> keys_;

  // access control
  // std::vector<int> access_permission_;
  // column mapping. some local_ columns do not need synced_ columns.
  std::vector<int> syn2loc_map_;
  std::vector<int> loc2syn_map_;
  int num_synced_vec_;

  // a better way?
  // map a keyrange into start index, end index, and keylist, invalid the caches
  // if keys are changed
  map<KeyRange, pair<IndexRange, KeyPtrRange> > key_positions_;
  map<KeyRange, RawArray> key_caches_;
  // store the temperal data used for aggregation
  map<int, EMat> aggregate_data_;
  bool vectors_inited_;
};

} // namespace PS
