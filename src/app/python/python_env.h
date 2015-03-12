#pragma once
#include <Python.h>
#include <boost/python.hpp>
#include <boost/numpy.hpp>
#include "python_bindings.h"

namespace PS {

class PythonEnv {
public:
  PythonEnv() : active_(false) {}

  boost::python::dict& globals() { return globals_; }

  void load_file(const char* path, int argc, char** argv) {
    reset();

    Py_Initialize();

    char* new_argv[argc];
    new_argv[0] = const_cast<char*>(path);
    for (int i = 1; i < argc; i++)
      new_argv[i] = argv[i];

    PySys_SetArgv(argc, new_argv);

    boost::python::object main_module = boost::python::import("__main__");
    globals_ = boost::python::dict(main_module.attr("__dict__"));

    boost::python::exec("import os, sys; sys.path = [''] + sys.path", globals_);
    //boost::python::exec("print(sys.path)");

    boost::numpy::initialize();
    initps();

    active_ = true;

    try {
      boost::python::exec_file(path, globals_);
    } catch (boost::python::error_already_set) {
      PyErr_Print();
      throw;
    }
  }

  void eval(const char* script) {
    try {
      boost::python::exec(script, globals_);
    } catch (boost::python::error_already_set) {
      PyErr_Print();
      throw;
    }
  }

  ~PythonEnv() {
    reset();
  }

protected:
  void reset() {
    if (active_) {
      globals_ = boost::python::dict();
      Py_Finalize();
    }
  }

private:
  bool active_;
  boost::python::dict globals_;
};

} // namespace PS

