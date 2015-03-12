#pragma once
#include <boost/python.hpp>
#include <boost/numpy.hpp>
#include "updater.h"
#include "python_env.h"

namespace PS {

template<typename V>
class PythonUpdater : public Updater<V> {
public:
  PythonUpdater(PythonEnv* py_env) : py_env_(py_env) { }
  virtual ~PythonUpdater() { }

  virtual void InitLayer(const std::string &name, V* weight, size_t size) {
    //LOG(ERROR) << "InitLayer size = " << size;
    try {

      Py_intptr_t shape[1] = { static_cast<Py_intptr_t>(size) };

      boost::numpy::ndarray py_weight = boost::numpy::zeros(1, shape, boost::numpy::dtype::get_builtin<float>());

      py_env_->globals().get("init_layer")(name, py_weight);

      memcpy(weight, py_weight.get_data(), sizeof(V) * size);

    } catch (boost::python::error_already_set) {
      //LOG(ERROR) << "InitLayer failed";
      PyErr_Print();
      throw;
    }
    //LOG(ERROR) << "InitLayer done";
  }

  virtual void Update(const std::string &name, V* weight, V* gradient, size_t size) {
    //LOG(ERROR) << "Update size = " << size;
    try {

      Py_intptr_t shape[1] = { static_cast<Py_intptr_t>(size) };

      boost::numpy::ndarray py_weight = boost::numpy::zeros(1, shape, boost::numpy::dtype::get_builtin<float>());
      memcpy(py_weight.get_data(), weight, sizeof(V) * size);

      boost::numpy::ndarray py_gradient = boost::numpy::zeros(1, shape, boost::numpy::dtype::get_builtin<float>());
      memcpy(py_gradient.get_data(), gradient, sizeof(V) * size);

      py_env_->globals().get("update_layer")(name, py_weight, py_gradient);

      memcpy(weight, py_weight.get_data(), sizeof(V) * size);

    } catch (boost::python::error_already_set) {
      //LOG(ERROR) << "Update failed";
      PyErr_Print();
      throw;
    }
    //LOG(ERROR) << "Update done";
  }

private:
  PythonEnv* py_env_;
};

} // namespace PS

