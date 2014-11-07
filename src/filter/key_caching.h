#pragma once
#include "filter/filter.h"
#include "util/crc32c.h"
namespace PS {

class KeyCachingFilter : public Filter {
  // thread safe
  void encode(const MessagePtr& msg) {
    // if (!msg->task.has_key_range()) return;
    auto conf = find(FilterConfig::KEY_CACHING, msg);
    if (!conf) return;
    if (!msg->hasKey()) {
      conf->clear_signature();
      return;
    }
    const auto& key = msg->key;
    auto sig = crc32c::Value(key.data(), std::min(key.size(), max_sig_len_));
    conf->set_signature(sig);
    auto cache_k = std::make_pair(
        msg->task.key_channel(), Range<Key>(msg->task.key_range()));
    Lock l(mu_);
    auto& cache = cache_[cache_k];
    bool hit_cache = cache.first == sig && cache.second.size() == key.size();
    if (hit_cache) {
      msg->clearKey();
    } else {
      cache.first = sig;
      cache.second = key;
    }
    if (conf->clear_cache_if_done() && isDone(msg->task)) {
      cache_.erase(cache_k);
    }
  }

  void decode(const MessagePtr& msg) {
    // if (!msg->task.has_key_range()) return;
    auto conf = find(FilterConfig::KEY_CACHING, msg);
    if (!conf || !conf->has_signature()) return;
    auto sig = conf->signature();
    // do a double check
    if (msg->hasKey()) {
      CHECK_EQ(crc32c::Value(msg->key.data(), std::min(msg->key.size(), max_sig_len_)), sig);
    }
    auto cache_k = std::make_pair(
        msg->task.key_channel(), Range<Key>(msg->task.key_range()));
    Lock l(mu_);
    auto& cache = cache_[cache_k];
    if (msg->hasKey()) {
      cache.first = sig;
      cache.second = msg->key;
    } else {
      // the cache is invalid... may ask the sender to resend this task
      CHECK_EQ(sig, cache.first) << msg->debugString();
      msg->setKey(cache.second);
    }
    if (conf->clear_cache_if_done() && isDone(msg->task)) {
      cache_.erase(cache_k);
    }
  }

 private:
  bool isDone(const Task& task) {
    return (!task.request() ||
            (task.has_shared_para()
             && task.shared_para().cmd() == CallSharedPara::PUSH));
  }

  std::unordered_map<
    std::pair<int, Range<Key>>, std::pair<uint32_t, SArray<char>>> cache_;

  // calculate the signature using the first max_sig_len_*4 bytes to accelerate
  // the computation
  const size_t max_sig_len_ = 2048;
  std::mutex mu_;
};

} // namespace
