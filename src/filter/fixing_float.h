#pragma once
#include "filter/filter.h"
#include <time.h>
namespace PS {

class FixingFloatFilter : public Filter {
 public:
  void encode(const MessagePtr& msg) {
    convert(msg, true);
  }

  void decode(const MessagePtr& msg) {
    convert(msg, false);
  }

 private:
  // a fast random function
  static bool boolrand(int* seed) {
    *seed = (214013 * *seed + 2531011);
    return ((*seed >> 16) & 0x1) == 0;
  }

  // decode / encode a message
  void convert(const MessagePtr& msg, bool encode) {
    auto filter_conf = CHECK_NOTNULL(find(FilterConfig::FIXING_FLOAT, msg));
    int n = msg->value.size();
    CHECK_EQ(n, msg->task.value_type_size());
    int k = 0;
    for (int i = 0; i < n; ++i) {
      auto type = msg->task.value_type(i);
      if (type == DataType::FLOAT) {
        CHECK_GT(filter_conf->fixed_point_size(), k);
        msg->value[i] = convert<float>(msg->value[i], encode, filter_conf->mutable_fixed_point(k++));
      }
      if (type == DataType::DOUBLE) {
        CHECK_GT(filter_conf->fixed_point_size(), k);
        msg->value[i] = convert<double>(msg->value[i], encode, filter_conf->mutable_fixed_point(k++));
      }
    }
  }

  // decode / encode an array
  template <typename V>
  SArray<char> convert(const SArray<char>& array, bool encode, FilterConfig::FixedFloatConfig* conf) {
    int nbytes = conf->num_bytes();
    CHECK_GT(nbytes, 0);
    CHECK_LT(nbytes, 8);
    double ratio = static_cast<double>(1 << (nbytes*8)) - 2;

    if (encode) {
      if (!conf->has_min_value()) {
        conf->set_min_value(SArray<V>(array).eigenArray().minCoeff());
      }
      if (!conf->has_max_value()) {
        conf->set_max_value(SArray<V>(array).eigenArray().maxCoeff() + 1e-6); // to avoid max_v == min_v
      }
    }

    CHECK(conf->has_min_value());
    double min_v = static_cast<double>(conf->min_value());
    CHECK(conf->has_max_value());
    double max_v = static_cast<double>(conf->max_value());
    double bin = max_v - min_v;
    CHECK_GT(bin, 0);

    if (encode) {
      // float/double to nbytes*8 int
      SArray<V> orig(array);
      SArray<uint8> code(orig.size() * nbytes);
      uint8* code_ptr = code.data();
      int seed = time(NULL);
      for (int i = 0; i < orig.size(); ++i) {
        double proj = orig[i] > max_v ? max_v : orig[i] < min_v ? min_v : orig[i];
        double tmp = (proj - min_v) / bin * ratio;
        uint64 r = static_cast<uint64>(floor(tmp)) + boolrand(&seed);
        for (int j = 0; j < nbytes; ++j) {
          *(code_ptr++) = static_cast<uint8>(r & 0xFF);
          r = r >> 8;
        }
      }
      return SArray<char>(code);
    } else {
      // nbytes*8 int to float/double
      uint8* code_ptr = SArray<uint8>(array).data();
      SArray<V> orig(array.size() / nbytes);
      for (int i = 0; i < orig.size(); ++i) {
        double r = 0;
        for (int j = 0; j < nbytes; ++j) {
          r += static_cast<uint64>(*(code_ptr++)) << 8 * j;
        }
        orig[i] = static_cast<V>(r / ratio * bin + min_v);
      }
      return SArray<char>(orig);
    }
  }
};

} // namespace PS
