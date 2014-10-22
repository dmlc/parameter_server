#pragma once
#include "filter/filter.h"

namespace PS {

class CompressingFilter : public Filter {
  void encode(const MessagePtr& msg) {
    auto conf = find(FilterConfig::COMPRESSING, msg);
    if (!conf) return;
    conf->clear_uncompressed_size();
    for (auto& v : msg->data) {
      conf->add_uncompressed_size(v.size());
      v = v.compressTo();
    }
  }
  void decode(const MessagePtr& msg) {
    auto conf = find(FilterConfig::COMPRESSING, msg);
    if (!conf) return;
    CHECK_EQ(conf->uncompress_size_size(), msg->data.size());
    for (int i = 0; i < msg->data.size(); ++i) {
      SArray<char> raw(conf->uncompress_size(i));
      raw.uncompressFrom(msg->data[i]);
      msg->data[i] = raw;
    }
  }
};
} // namespace PS
