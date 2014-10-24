#pragma once
#include "filter/filter.h"

namespace PS {

class CompressingFilter : public Filter {
  void encode(const MessagePtr& msg) {
    auto conf = find(FilterConfig::COMPRESSING, msg);
    if (!conf) return;
    conf->clear_uncompressed_size();
    if (msg->hasKey()) {
      conf->add_uncompressed_size(msg->key.size());
      msg->key = msg->key.compressTo();
    }
    for (auto& v : msg->value) {
      conf->add_uncompressed_size(v.size());
      v = v.compressTo();
    }
  }
  void decode(const MessagePtr& msg) {
    auto conf = find(FilterConfig::COMPRESSING, msg);
    if (!conf) return;
    int has_key = msg->hasKey();
    CHECK_EQ(conf->uncompressed_size_size(), msg->value.size() + has_key);

    if (has_key) {
      SArray<char> raw(conf->uncompressed_size(0));
      raw.uncompressFrom(msg->key);
      msg->key = raw;
    }
    for (int i = 0; i < msg->value.size(); ++i) {
      SArray<char> raw(conf->uncompressed_size(i+has_key));
      raw.uncompressFrom(msg->value[i]);
      msg->value[i] = raw;
    }
  }
};

} // namespace PS
