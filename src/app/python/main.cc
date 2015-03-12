#include "ps.h"
#include "python_env.h"
#include "python_server.h"

namespace PS {

DEFINE_string(script, "", "the Python script path");

App* CreateServerNode(const std::string& conf) {
  static int argc = 2;
  static std::string s_conf = conf;
  static char* argv[2] = { const_cast<char*>(""), const_cast<char*>(s_conf.c_str()) };

  PS::PythonEnv* py_env = new PS::PythonEnv();
  py_env->load_file(PS::FLAGS_script.c_str(), argc, argv);

  return new PythonServer(py_env);
}

} // namespace PS

int WorkerNodeMain(int argc, char *argv[]) {
  PS::PythonEnv py_env;
  py_env.load_file(PS::FLAGS_script.c_str(), argc, argv);

  try {
    py_env.globals().get("worker_node_main")();
  } catch (boost::python::error_already_set) {
    PyErr_Print();
    throw;
  }

  return 0;
}

