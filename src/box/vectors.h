#pragma once

#include "util/common.h"
#include "util/xarray.h"
#include "box/container.h"
#include "util/eigen3.h"

namespace PS {

template <typename V>
class Vectors : public Container {
 public:
  typedef Eigen::Matrix<V, Eigen::Dynamic, Eigen::Dynamic> EMat;
  typedef Eigen::Matrix<V, Eigen::Dynamic, 1> EVec;
  typedef std::initializer_list<int> VecList;
  // todo, man ask global length from the postmaster
  Vectors(string      name,
          size_t      global_length,
          int         num_vec      = 1,
          XArray<Key> key_list     = XArray<Key>());

  Status Push(KeyRange key_range = KeyRange::All(),
              VecList  vec_list  = {1},
              bool     delta     = kDelta,
              uid_t    dest      = kServer);

  Status Pull(KeyRange key_range = KeyRange::All(),
              VecList  vec_list  = {1},
              bool     delta     = kValue,
              uid_t    dest      = kServer);

  Status PushPull(KeyRange key_range     = KeyRange::All(),
                  VecList  push_vec_list = {0},
                  bool     push_delta    = kDelta,
                  VecList  pull_vec_list = {0},
                  bool     pull_delta    = kValue,
                  uid_t    dest          = kServer);

  Status GetLocalData(Mail *mail);
  Status MergeRemoteData(const Mail& mail);

  // return the i-th vector
  EMat vecs() { return local_; }

  Eigen::Block<EMat, EMat::RowsAtCompileTime, 1> Vec(int i) {
    return Eigen::Block<EMat, EMat::RowsAtCompileTime, 1>(local_, i);
  }

 private:
  size_t vec_len_;
  int num_vec_;
  EMat local_;
  EMat synced_;
  XArray<Key> keys_;

  // a better way?
  // map a keyrange into start index, end index, and keylist, invalid the caches
  // if keys are changed
  map<KeyRange, std::pair<size_t, size_t>> key_indices_;
  map<KeyRange, RawArray> key_caches_;
};


template <typename V>
Vectors<V>::Vectors(string name, size_t global_length,
                    int num_vec, XArray<Key> key_list)
    : Container(name), num_vec_(num_vec) {
  CHECK_GT(num_vec, 0);
  Container::Init(KeyRange(0, global_length));
  // fill  keys
  if (key_list.Empty()) {
    vec_len_ = key_range_.Size();
    LL << "vec_len:" << vec_len_ << std::endl;
    keys_ = XArray<Key>(vec_len_);
    for (size_t i = 0; i < vec_len_; ++i) {
      keys_[i] = key_range_.start() + i;
    }
  } else {
    CHECK(postmaster_->IamClient());
    vec_len_ = key_list.size();
    keys_ = key_list;
  }
  // fill values
  local_ = EMat::Zero(vec_len_, num_vec);
  synced_ = local_;


}


template <typename V>
Status Vectors<V>::Push(KeyRange key_range, VecList vec_list,
                        bool delta, uid_t dest) {
  CHECK(0);
  return Status::OK();
}


template <typename V>
Status Vectors<V>::PushPull(KeyRange key_range, VecList push_vec_list,
                            bool push_delta, VecList  pull_vec_list,
                            bool pull_delta, uid_t dest) {
  if (key_range == KeyRange::All()) key_range = key_range_;
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
  for (int v : push_vec_list)
    push->add_vectors(v);
  // pull
  auto pull = head.mutable_pull();
  pull->set_delta(pull_delta);
  for (int v : pull_vec_list)
    pull->add_vectors(v);
  // do it
  Container::Push(head);
  return Status::OK();
}

template <typename V>
Status Vectors<V>::Pull(KeyRange key_range, VecList vec_list,
                        bool delta, uid_t dest) {
  CHECK(0);
  return Status::OK();
}


template<class V>
Status Vectors<V>::GetLocalData(Mail *mail) {
  // fill the keys, first check the cache, otherwise construct it.
  // TODO a better way to avoid the memcpy here
  Header& head = mail->flag();
  KeyRange kr(head.key().start(), head.key().end());
  RawArray keys;
  size_t start_idx, end_idx;
  auto it = key_caches_.find(kr);
  if (it == key_caches_.end()) {
    Key* start = keys_.UpperBound(kr.start(), &start_idx);
    Key* end = keys_.LowerBound(kr.end()-1, &end_idx);
    ++end_idx;
    size_t n = end_idx - start_idx;
    CHECK(start != NULL);
    CHECK(end != NULL);
    CHECK_GT(end_idx, start_idx);
    CHECK_EQ(n-1, (size_t)(end-start));
    keys = RawArray(sizeof(Key), n);
    memcpy(keys.data(), start,  keys.size());
    key_caches_[kr] = keys;
    key_indices_[kr] = make_pair(start_idx, end_idx);
  } else {
    keys = it->second;
    auto it2 = key_indices_.find(kr);
    CHECK(it2 != key_indices_.end());
    start_idx = it2->second.first;
    end_idx = it2->second.second;
  }
  head.mutable_key()->set_cksum(keys.ComputeCksum());
  mail->set_keys(keys);
  // fill the values
  int nvec = head.push().vectors_size();
  CHECK_GT(nvec, 0);
  size_t nkeys = keys.entry_num();
  XArray<V> vals(nkeys*nvec);
  for (size_t i = 0; i < nkeys; ++i) {
    for (int j = 0; j < nvec; ++j) {
      int x = head.push().vectors(j);
      size_t y = i * nvec + j;
      if (head.push().delta()) {
        vals[y] = local_.coeff(i+start_idx, x) - synced_.coeff(i+start_idx, x);
      } else {
        vals[y] = local_.coeff(i+start_idx, x);
      }
    }
  }
  head.mutable_value()->set_empty(false);
  mail->set_vals(vals.raw());
  return Status::OK();
}

template<class V>
Status Vectors<V>::MergeRemoteData(const Mail& mail) {
  // find the key segment
  const Header& head = mail.flag();
  KeyRange kr(head.key().start(), head.key().end());
  auto it = key_indices_.find(kr);
  size_t start_idx, end_idx;
  if (it == key_indices_.end()) {
    keys_.UpperBound(kr.start(), &start_idx);
    keys_.LowerBound(kr.end(), &end_idx);
  } else {
    start_idx = it->second.first;
    end_idx = it->second.second;
  }
  // merge the data
  XArray<Key> keys(mail.keys());
  XArray<V> vals(mail.vals());
  int nvec = head.push().vectors_size();
  CHECK_GT(nvec, 0);
  CHECK_EQ(keys.size()*nvec, vals.size()) << " I'm " << SName();
  bool delta = head.push().delta() ||
               (aggregator_.ValidDefault() && !aggregator_.IsFirst(head.time()));
  size_t i = 0, j = start_idx;
  while (i < keys.size() && j < end_idx) {
    if (keys[i] < keys_[j]) {
      ++ i;
      continue;
    } else if (keys[i] > keys_[j]) {
      ++ j;
      continue;
    }
    // now matched
    for (int k = 0; k < nvec; ++k) {
      int x = head.push().vectors(k);
      V v = vals[i*nvec+k];
      if(delta) {
        local_.coeffRef(j, x) += v;
        synced_.coeffRef(j, x) += v;
      } else {
        local_.coeffRef(j, x) += (v - synced_.coeff(j, x));
        synced_.coeffRef(j, x) = v;
      }
    }
    ++i; ++j;
  }

  // LL << SName() << "merge: " << local_.norm() << " " << synced_.norm();
  return Status::OK();
}

} // namespace PS
