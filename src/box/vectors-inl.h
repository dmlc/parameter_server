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
                    const XArray<Key>& global_keys) : Container(name)  {
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
Status Vectors<V>::GetLocalData(Mail *mail) {
  // 1. fill the keys, first check whether hit the cache. if not, construct it.
  Header& head = mail->flag();
  KeyRange kr(head.key().start(), head.key().end());
  RawArray keys;
  size_t start_idx, end_idx;
  auto it = key_caches_.find(kr);
  if (it == key_caches_.end()) {
    // miss the cache
    Key* start = keys_.UpperBound(kr.start(), &start_idx);
    Key* end = keys_.LowerBound(kr.end()-1, &end_idx);
    ++end_idx;
    // LL << kr.ToString() << " " << start_idx << " " << end_idx;
    size_t n = end_idx - start_idx;
    CHECK(start != NULL);
    CHECK(end != NULL);
    CHECK_GT(end_idx, start_idx);
    CHECK_EQ(n-1, (size_t)(end-start));
    keys = RawArray(sizeof(Key), n);
    // TODO a better way to avoid the memcpy here
    memcpy(keys.data(), start,  keys.size());
    // set the cache
    key_caches_[kr] = keys;
    key_indices_[kr] = make_pair(start_idx, end_idx);
  } else {
    // hit the cache
    keys = it->second;
    auto it2 = key_indices_.find(kr);
    CHECK(it2 != key_indices_.end());
    start_idx = it2->second.first;
    end_idx = it2->second.second;
  }
  head.mutable_key()->set_cksum(keys.ComputeCksum());
  mail->set_keys(keys);

  // 2. fill the values. It is a #keys x #vec matrix, storing in a column-majored
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
        vals[v*nkeys+i] = local_.coeff(i+start_idx, x)
                          - synced_.coeff(i+start_idx, z);
      }
    } else {
      for (size_t i = 0; i < nkeys; ++i) {
        vals[v*nkeys+i] = local_.coeff(i+start_idx, x);
      }
    }
  }
  head.mutable_value()->set_empty(false);
  mail->set_vals(vals.raw());

  XArray<Key> xkey(keys);
  // LL << "to " << mail->flag().recver() << " keys: " << xkey.DebugString();
  // LL << "to " << mail->flag().recver() << " vals: " << vals.DebugString();

  return Status::OK();
}

template<class V>
Status Vectors<V>::MergeRemoteData(const Mail& mail) {
  // 1. find the key range
  const Header& head = mail.flag();
  KeyRange kr(head.key().start(), head.key().end());
  auto it = key_indices_.find(kr);
  size_t start_idx, end_idx;
  if (it == key_indices_.end()) {
    keys_.UpperBound(kr.start(), &start_idx);
    keys_.LowerBound(kr.end(), &end_idx);
    ++ end_idx;
  } else {
    start_idx = it->second.first;
    end_idx = it->second.second;
  }

  // 2. construct the key mapping between remote and local

  // 2. merge the data
  XArray<Key> keys(mail.keys());
  XArray<V> vals(mail.vals());

  // LL << kr.ToString() << " " << start_idx << " " << end_idx;

  int nvec = head.push().vectors_size();
  int nkey = keys.size();
  CHECK_GT(nvec, 0);
  CHECK_EQ(nkey*nvec, vals.size());
  // LL << head.push().delta();
  // LL << aggregator_.ValidDefault();
  // LL << aggregator_.IsFirst(head.time());

  bool delta = head.push().delta() ||
               (aggregator_.ValidDefault() && !aggregator_.IsFirst(head.time()));

  for (int v = 0; v < nvec; ++v) {
    int x = head.push().vectors(v);
    int z = loc2syn_map_[x];
    bool has_sync = z > -1;

    size_t i = 0, j = start_idx;
    while (i < nkey && j < end_idx) {
      if (keys[i] < keys_[j]) {
        LL << "ignore received key " << keys[i];
        ++ i;
        continue;
      } else if (keys[i] > keys_[j]) {
        ++ j;
        continue;
      }
      // now key matched
      V val = vals[v*nkey+i];
      if (delta) {
        local_.coeffRef(j, x) += val;
        if (has_sync)
          synced_.coeffRef(j, z) += val;
      } else {
        if (has_sync) {
          local_.coeffRef(j, x) += (val - synced_.coeff(j, z));
          synced_.coeffRef(j, z) = val;
        } else {
          local_.coeffRef(j, x) = val;
        }
      }
      ++i; ++j;
    }
  }

  LL << "time: " << mail.flag().time() << ", merge data from "
     << mail.flag().sender() << " "
     << mail.keys().size() << "(" << !mail.flag().key().empty() <<  ") keys and "
     << mail.vals().size() << "(" << !mail.flag().value().empty() << ") vals";
  LL << "keys: " << keys.DebugString();
  LL << "vals: " << vals.DebugString();
  LL << DebugString();
  return Status::OK();
}

} // namespace PS
