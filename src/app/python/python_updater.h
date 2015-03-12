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

      auto shape = boost::python::make_tuple(size);
      auto stride = boost::python::make_tuple(sizeof(V));

      // construct new ndarrays using the existing C arrays without copying
      auto py_weight = boost::numpy::from_data(weight, boost::numpy::dtype::get_builtin<V>(), shape, stride, boost::python::object());

      py_env_->globals().get("server_init_layer")(name, py_weight);

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

      auto shape = boost::python::make_tuple(size);
      auto stride = boost::python::make_tuple(sizeof(V));

      // construct new ndarrays using the existing C arrays without copying
      auto py_weight = boost::numpy::from_data(weight, boost::numpy::dtype::get_builtin<V>(), shape, stride, boost::python::object());
      auto py_gradient = boost::numpy::from_data(gradient, boost::numpy::dtype::get_builtin<V>(), shape, stride, boost::python::object());

      py_env_->globals().get("server_update_layer")(name, py_weight, py_gradient);

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

