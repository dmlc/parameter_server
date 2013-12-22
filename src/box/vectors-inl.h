#pragma once

#include "box/vectors.h"

// // access permission
// static const int kRead = 0;
// static const int kWrite = 1;
// static const int kReadWrite = 2;

namespace PS {

template <typename V>
Vectors<V>::Vectors(const string& name,
                    size_t global_length,
                    int num_vec,
                    const XArray<Key>& global_keys)
    : Container(name), vectors_inited_(false)  {
  num_vec_ = num_vec;
  // CHECK_GT(num_vec_, 0);
  // for (int i = 0; i < num_vec_; ++i ) {
  //   access_permission_.push_back(kReadWrite);
  // }
  Init(global_length, global_keys);
}

// template <typename V>
// Vectors<V>::Vectors(const string& name,
//                     size_t global_length,
//                     const IntList& access_permission,
//                     const XArray<Key>& global_keys) : Container(name)  {
//   num_vec_ = access_permission.size();
//   CHECK_GT(num_vec_, 0);
//   for (auto p : access_permission) {
//     access_permission_.push_back(p);
//   }
//   Init(global_length, global_keys);
// }

// TODO block other theads if init is not finished.
template <typename V>
void Vectors<V>::Init(size_t global_length, const XArray<Key>& global_keys) {
  Container::Init(KeyRange(0, global_length));
  // fill keys
  if (global_keys.Empty()) {
    vec_len_ = key_range_.size();
    keys_ = XArray<Key>(vec_len_);
    for (size_t i = 0; i < vec_len_; ++i) {
      keys_[i] = key_range_.start() + i;
    }
  } else {
    CHECK(postmaster_->IamClient());
    vec_len_ = global_keys.size();
    keys_ = global_keys;
  }
  // construct mapping between local_ and synced_
  // TODO a naive version here
  num_synced_vec_ = num_vec_;
  loc2syn_map_.clear();
  syn2loc_map_.clear();
  for (int i = 0; i < num_vec_; ++i) {
    loc2syn_map_.push_back(i);
    syn2loc_map_.push_back(i);
  }
  // num_synced_vec_ = 0;
  // loc2syn_map_.resize(num_vec_);
  // syn2loc_map_.clear();
  // int i = 0;
  // for (auto p : access_permission_) {
  //   if (p == kReadWrite) {
  //     loc2syn_map_[i] = num_synced_vec_;
  //     syn2loc_map_.push_back(i);
  //     num_synced_vec_ ++;
  //   } else {
  //     loc2syn_map_[i] = -1;
  //   }
  //   ++ i;
  // }

  // fill values
  // TODO nonzero initialization?
  local_ = EMat::Zero(vec_len_, num_vec_);
  synced_ = EMat::Zero(vec_len_, num_synced_vec_);
  vectors_inited_ = true;
}

template <typename V>
void Vectors<V>::WaitInited() {
  Container::WaitInited();
  while (!vectors_inited_) {
    LL << "waiting vectors(" << name() << ") is initialized";
    std::this_thread::sleep_for(milliseconds(100));
  }
}

template <typename V>
Status Vectors<V>::Push(KeyRange key_range, IntList vec_list,
                        bool delta, uid_t dest) {
  CHECK(0);
  return Status::OK();
}


template <typename V>
Status Vectors<V>::PushPull(KeyRange key_range, IntList push_vec_list,
                            bool push_delta, IntList  pull_vec_list,
                            bool pull_delta, uid_t dest) {
  // 1. set the header
  if (key_range == KeyRange::All())
    key_range = key_range_;
  CHECK(key_range.Valid());
  Header head;
  head.set_type(Header::PUSH_PULL);
  head.set_recver(dest);
  // key range
  auto k = head.mutable_key();
  k->set_start(key_range.start());
  k->set_end(key_range.end());
  // push
  auto push = head.mutable_push();
  push->set_delta(push_delta);
  for (int v : push_vec_list) {
    // CHECK_NE(access_permission_[v], kRead)
    //     << " vec[" << v << "] is local read-only, you cannot push it."
    //     << " Change it to kReadWrite or kWrite.";
    push->add_vectors(v);
  }
  // pull
  auto pull = head.mutable_pull();
  pull->set_delta(pull_delta);
  for (int v : pull_vec_list) {
    // CHECK_NE(access_permission_[v], kWrite)
    //     << " vec[" << v << "] is local write-only, you cannot pull it."
    //     << " Change it to kReadWrite or kRead.";
    pull->add_vectors(v);
  }
  // 2. do it
  Container::Push(head);
  return Status::OK();
}

template <typename V>
Status Vectors<V>::Pull(KeyRange key_range, IntList vec_list,
                        bool delta, uid_t dest) {
  CHECK(0);
  return Status::OK();
}

template<class V>
IndexRange Vectors<V>::FindIndex(KeyRange kr, KeyPtrRange* key_ptr) {
  IndexRange idx;
  KeyPtrRange ptr;
  auto it = key_positions_.find(kr);
  if (it == key_positions_.end()) {
    ptr.start() = keys_.UpperBound(kr.start(), &idx.start());
    ptr.end() = keys_.LowerBound(kr.end(), &idx.end());
    CHECK(ptr.start() != NULL);
    CHECK(ptr.end() != NULL);
    CHECK_EQ(ptr.size(), idx.size());
    ++ idx.end();
    key_positions_[kr] = make_pair(idx, ptr);
  } else {
    idx = it->second.first;
    ptr = it->second.second;
  }
  if (key_ptr != NULL)
    *key_ptr = ptr;
  return idx;
}

template<class V>
Status Vectors<V>::GetLocalData(Mail *mail) {
  // 1. fill the keys, first check whether hit the cache. if not, construct it.
  Header& head = mail->flag();
  KeyRange kr(head.key().start(), head.key().end());
  Range<Key*> ptr;
  IndexRange idx = FindIndex(kr, &ptr);
  RawArray keys;
  auto it = key_caches_.find(kr);
  if (it == key_caches_.end()) {
    // miss the cache
    CHECK(ptr.Valid());
    keys = RawArray(sizeof(Key), idx.size());
    // TODO find a better way to avoid the memcpy here
    memcpy(keys.data(), keys_.data()+idx.start(),  keys.size());
    // set the cache
    key_caches_[kr] = keys;
  } else {
    // hit the cache
    keys = it->second;
  }
  head.mutable_key()->set_cksum(keys.ComputeCksum());
  mail->set_keys(keys);

  // 2. fill the values. It is a #keys x #vec matrix, storing in a row-majored
  // format
  int nvec = head.push().vectors_size();
  CHECK_GT(nvec, 0);
  size_t nkeys = keys.entry_num();
  XArray<V> vals(nkeys * nvec);
  for (int v = 0; v < nvec; ++v) {
    int x = head.push().vectors(v);
    if (head.push().delta()) {
      // CHECK_EQ(access_permission_[x], kReadWrite) << "vec[" << x <<
      //     "] should be kReadWrite if you want to push its delta values";
      int z = loc2syn_map_[x];
      CHECK_GE(z, 0); CHECK_LT(z, num_synced_vec_);
      for (size_t i = 0; i < nkeys; ++i) {
        vals[i*nvec+v] = local_.coeff(i+idx.start(), x)
                          - synced_.coeff(i+idx.start(), z);
      }
    } else {
      for (size_t i = 0; i < nkeys; ++i) {
        vals[i*nvec+v] = local_.coeff(i+idx.start(), x);
      }
    }
  }
  head.mutable_value()->set_empty(false);
  vals.raw().ResetEntrySize(nvec*sizeof(V));
  mail->set_vals(vals.raw());

#ifdef DEBUG_VECTORS_
  XArray<Key> xkey(keys);
  LL << "to " << mail->flag().recver() << " keys: " << xkey.DebugString();
  LL << "to " << mail->flag().recver() << " vals: " << vals.DebugString();
#endif

  return Status::OK();
}

template<class V>
Status Vectors<V>::MergeRemoteData(const Mail& mail) {
  // 1. mapping the key range into local indeces
  const Header& head = mail.flag();
  KeyRange kr(head.key().start(), head.key().end());
  IndexRange idx = FindIndex(kr);

  // 2. construct the key mapping between remote and local
  XArray<Key> keys(mail.keys());
  size_t nkey = keys.size();
  // LL << idx.ToString() << " " << idx.size();
  std::vector<size_t> remote_idx(idx.size()), local_idx(idx.size());
  size_t idx_len = 0;
  for (size_t i = 0, j = idx.start(); i < nkey && j < idx.end(); ) {
    if (keys[i] < keys_[j]) {
      LL << "ignore received key " << keys[i];
      ++ i;
      continue;
    } else if (keys[i] > keys_[j]) {
      ++ j;
      continue;
    }
    // now key matched
    remote_idx[idx_len] = i;
    local_idx[idx_len] = j - idx.start();
    ++ idx_len;
    ++ i;
    ++ j;
  }
  // 3. merge the data
  XArray<V> vals(mail.vals());
  int nvec = head.push().vectors_size();
  CHECK_GT(nvec, 0);
  CHECK_EQ(nkey*nvec, vals.size());
  int time = head.time();
  bool delta = head.push().delta();
  // two different cases
  if (aggregator_.Valid(time)) {
    // if there is a valid aggregator, first do aggregation in a temporal place.
    if (aggregator_.IsFirst(time))
      aggregate_data_[time] = EMat::Zero(idx.size(), nvec);
    auto& mat = aggregate_data_[time];
    CHECK_EQ(mat.rows(), (int)idx.size());
    CHECK_EQ(mat.cols(), nvec);
    for (size_t i = 0; i < idx_len; ++i)
      for (int v = 0; v < nvec; ++v)
        mat.coeffRef(local_idx[i],v) += vals[remote_idx[i]*nvec+v];
    if (AggregateSuccess(time)) {
      // if aggregation is complete, then move temporal data to local_ and sync_
      for (int v = 0; v < nvec; ++v) {
        int x = head.push().vectors(v);
        int z = loc2syn_map_[x];
        bool valid_sync = z >= 0 && z < synced_.cols();
        if (delta) {
          local_.col(x).segment(idx.start(), idx.size()) += mat.col(v);
          if (valid_sync) {
            synced_.col(z).segment(idx.start(), idx.size()) += mat.col(v);
          }
        } else {
          if (valid_sync) {
            local_.col(x).segment(idx.start(), idx.size()) +=
                (mat.col(v) - synced_.col(z).segment(idx.start(), idx.size()));
            synced_.col(z).segment(idx.start(), idx.size()) = mat.col(v);
          } else {
            local_.col(x).segment(idx.start(), idx.size()) = mat.col(v);
          }
        }
      }
    }
  } else {
    // do merge directly
    for (int v = 0; v < nvec; ++v) {
      int x = head.push().vectors(v);
      int z = loc2syn_map_[x];
      bool valid_sync = z >= 0 && z < synced_.cols();
      for (size_t i = 0; i < idx_len; ++i) {
        V val = vals[remote_idx[i]*nvec+v];
        size_t y = local_idx[i] + idx.start();
        if (delta) {
          local_.coeffRef(y, x) += val;
          if (valid_sync)
            synced_.coeffRef(y, z) += val;
        } else {
          if (valid_sync) {
            local_.coeffRef(y, x) += (val - synced_.coeff(y, z));
            synced_.coeffRef(y, z) = val;
          } else {
            local_.coeffRef(y, x) = val;
          }
        }
      }
    }
  }

#ifdef DEBUG_VECTORS_
  LL << "time: " << mail.flag().time() << ", merge data from "
     << mail.flag().sender() << " "
     << mail.keys().size() << "(" << !mail.flag().key().empty() <<  ") keys and "
     << mail.vals().size() << "(" << !mail.flag().value().empty() << ") vals";
  LL << "keys: " << keys.DebugString();
  LL << "vals: " << vals.DebugString();
  LL << DebugString();
#endif
  return Status::OK();
}

} // namespace PS
