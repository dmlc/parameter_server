#pragma once

namespace PS {

template<typename V>
class Updater {
public:
  Updater() { }
  virtual ~Updater() { }

  virtual void InitLayer(const std::string &name, V* weight, size_t size) { }

  virtual void Update(const std::string &name, V* weight, V* gradient, size_t size) { }
};

} // namespace PS
