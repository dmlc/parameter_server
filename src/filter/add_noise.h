#pragma once
#include "filter/filter.h"
namespace PS {

/**
 * @brief Add noise
 *
 */
class AddNoiseFilter : public Filter {
 public:
  void encode(Message* msg) {
    auto filter_conf = CHECK_NOTNULL(find(FilterConfig::NOISE, msg));
    int n = msg->value.size();
    CHECK_EQ(n, msg->task.value_type_size());
    for (int i = 0; i < n; ++i) {
      if (msg->value[i].size() == 0) continue;
      auto type = msg->task.value_type(i);
      if (type == DataType::FLOAT) {
        AddNoise<float>(msg->value[i], filter_conf);
      }
      if (type == DataType::DOUBLE) {
        AddNoise<double>(msg->value[i], filter_conf);
      }
    }
  }

 private:

  template <typename V>
  void AddNoise(const SArray<char>& array, FilterConfig* cf) {
    std::default_random_engine generator;
    std::normal_distribution<V> distribution((V)cf->mean(), (V)cf->std());
    SArray<V> data(array);
    // SArray<V> noise(data.size());
    for (size_t i = 0; i < data.size(); ++i) {
      data[i] += distribution(generator);
    }
    // LL << noise.Std() << " " << noise;
  }

};

}  // namespace PS
