#pragma once
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#include <boost/python.hpp>
#pragma GCC diagnostic pop

namespace PS {

class PythonEnv {
public:
  PythonEnv() : active_(false) {}

  boost::python::dict& globals() { return globals_; }

  void load_file(const char* path, int argc, char** argv);

  void eval(const char* script);

  ~PythonEnv() {
    reset();
  }

protected:
  void reset();

private:
  bool active_;
  boost::python::dict globals_;
};

} // namespace PS

