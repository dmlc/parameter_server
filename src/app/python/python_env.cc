#include "python_env.h"
#include <Python.h>
#include <boost/numpy.hpp>
#include "python_bindings.h"

namespace PS {

void PythonEnv::load_file(const char* path, int argc, char** argv) {
  reset();

  // initialize the Python interpreter
  Py_Initialize();

  // obtain globals
  boost::python::object main_module = boost::python::import("__main__");
  globals_ = boost::python::dict(main_module.attr("__dict__"));

  // construct a new argument list, with the first one replaced by the script path to enable module imports in the same directory
  char* new_argv[argc];
  new_argv[0] = const_cast<char*>(path);
  for (int i = 1; i < argc; i++)
    new_argv[i] = argv[i];

  PySys_SetArgv(argc, new_argv);

  // add '' to sys.path as well
  boost::python::exec("import os, sys; sys.path = [''] + sys.path", globals_);
  //boost::python::exec("print(sys.path)");

  // initialize boost::numpy and our own bindings
  boost::numpy::initialize();
  init_bindings();

  active_ = true;

  try {
    boost::python::exec_file(path, globals_);
  } catch (boost::python::error_already_set) {
    PyErr_Print();
    throw;
  }
}

void PythonEnv::eval(const char* script) {
  try {
    boost::python::exec(script, globals_);
  } catch (boost::python::error_already_set) {
    PyErr_Print();
    throw;
  }
}

void PythonEnv::reset() {
  if (active_)
    Py_Finalize();
}

} // namespace PS

