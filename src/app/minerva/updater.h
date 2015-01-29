#pragma once

namespace minerva {

template<typename V>
class Updater {
 public:
  Updater() { }
  virtual ~Updater() { }

  virtual void InitLayer(const std::string &name, V* weight, size_t size) {
    // init by 0, gaussian random, or others
    for (int i = 0; i < size; ++i) {
      weight[i] = 0;
    }
  }

  virtual void Update(const std::string &name, V* weight, V* gradient, size_t size) {
    // weight -= eta * gradient
    V eta = .1;
    for (int i = 0; i < size; ++i) {
      weight[i] -= eta * gradient[i];
    }
  }
};

} // namespace minerva
