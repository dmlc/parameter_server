#pragma once

#include "minerva_ps.h"

namespace PS {
  namespace minerva{

template<typename V>
class Updater {
public:
  Updater() { }
  virtual ~Updater() { }

  virtual void InitLayer(const std::string &name, V* weight, size_t size) {
    ::InitLayer(name, weight, size);
  }

  virtual void Update(const std::string &name, V* weight, V* gradient, size_t size) {
    ::UpdateLayer(name, weight, gradient, size);
  }
};

  } // namespace minerva
} // namespace PS
