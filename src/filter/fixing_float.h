#pragma once
#include "filter/filter.h"
#include <time.h>
namespace PS {

class FixingFloatFilter : public Filter {
 public:
  void encode(const MessagePtr& msg) {
    auto conf = CHECK_NOTNULL(find(FilterConfig::FIXING_FLOAT, msg))->fixed_point();
    int n = msg->value.size();
    CHECK_EQ(n, msg->task.value_type_size());
    for (int i = 0; i < n; ++i) {
      auto type = msg->task.value_type(i);
      if (type == DataType::FLOAT) {
        msg->value[i] = encode(SArray<float>(msg->value[i]), conf);
      }
      if (type == DataType::DOUBLE) {
        msg->value[i] = encode(SArray<double>(msg->value[i]), conf);
      }
    }
  }

  void decode(const MessagePtr& msg) {
    auto conf = CHECK_NOTNULL(find(FilterConfig::FIXING_FLOAT, msg))->fixed_point();
    int n = msg->value.size();
    CHECK_EQ(n, msg->task.value_type_size());
    for (int i = 0; i < n; ++i) {
      auto type = msg->task.value_type(i);
      if (type == DataType::FLOAT) {
        msg->value[i] = decode<float>(msg->value[i], conf);
      }
    }
  }

 private:
  inline bool boolrand(int* seed) {
    *seed = (214013 * *seed + 2531011);
    return ((*seed >> 16) & 0x1) == 0;
  }

  template <typename V>
  SArray<char> encode(const SArray<V>& data, const FilterConfig::FixedConfig& conf) {
    int nbytes = conf.num_bytes();
    CHECK_GT(nbytes, 0);
    CHECK_LT(nbytes, 8);
    V ratio = static_cast<V>(1<<(nbytes*4));
    V min_v = static_cast<V>(conf.min_value());
    V max_v = static_cast<V>(conf.max_value());
    V bin = max_v - min_v;
    CHECK_GT(bin, 0);

    SArray<char> res(data.size() * nbytes);
    char* res_ptr = res.data();
    int seed = time(NULL);

    for (int i = 0; i < data.size(); ++i) {
      V proj = data[i] > max_v ? max_v : data[i] < min_v ? min_v : data[i];
      V tmp = (proj - min_v) / bin * ratio;
      uint64 r = static_cast<uint64>(floor(tmp)) + boolrand(&seed);

      for (int j = 0; j < nbytes; ++j) {
        *(res_ptr++) = static_cast<char>(r & 0xFF);
        r = r >> 8;
      }
    }
    return res;
  }

  template <typename V>
  SArray<V> decode(const SArray<char>& data, const FilterConfig::FixedConfig& conf) {
    int nbytes = conf.num_bytes();
    V ratio = static_cast<V>(1<<(nbytes*4));
    V min_v = static_cast<V>(conf.min_value());
    V max_v = static_cast<V>(conf.max_value());
    V bin = max_v - min_v;

    int n = data.size() / nbytes;
    SArray<V> res(n);
    char* data_ptr = data.data();
    for (int i = 0; i < n; ++i) {
      V r = 0;
      for (int j = 0; j < nbytes; ++j) {
        r += static_cast<uint64>(*(data_ptr++)) << 8 * j;
      }
      res[i] = static_cast<V>(r) / ratio * bin + min_v;
    }
    return res;
  }
};

} // namespace PS
