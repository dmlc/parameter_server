/**
 * @file   ps-inl.h
 * @brief  Implementation of ps.h
 */
#pragma once
#include "ps.h"
#include "kv/kv_cache.h"
namespace ps {

/// worker nodes

template<typename V>
KVWorker<V>::KVWorker(int id) {
  cache_ = CHECK_NOTNULL((new KVCache<Key, V>(id)));
}

template<typename V>
KVWorker<V>::~KVWorker() {
  delete cache_;
}

template<typename V>
void KVWorker<V>::Wait(int timestamp) {

}


template<typename V>
int KVWorker<V>::Push(CBlob<Key> keys, CBlob<V> values, const SyncOpts& opts) {
  return 0;
}

template<typename V>
int KVWorker<V>::Pull(CBlob<Key> keys, Blob<V> values, const SyncOpts& opts) {

  return 0;
}


template<typename V>
int KVWorker<V>::Push(const SBlob<Key>& keys, const SBlob<V>& values,
                     const SyncOpts& opts) {
  return cache_->Push(keys, values, opts);
}

template<typename V>
int KVWorker<V>::Pull(const SBlob<Key>& keys, SBlob<V>* values,
                     const SyncOpts& opts) {

  return 0;
}


template<typename V>
void KVWorker<V>::IncrClock(int delta) {

}

/// server nodes

}  // namespace ps
