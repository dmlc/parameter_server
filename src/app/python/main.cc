#include "ps.h"
#include "python_env.h"
#include "python_server.h"

namespace PS {

DEFINE_string(script, "", "the Python script path");

App* CreateServerNode(const std::string& conf) {
  std::string script = FLAGS_script;

  return new PythonServer(script, conf);
}

} // namespace PS

int WorkerNodeMain(int argc, char *argv[]) {
  std::string script = PS::FLAGS_script;

  PS::PythonEnv py_env;
  py_env.load_file(script.c_str(), argc, argv);

  try {
    if (py_env.globals().has_key("worker_node_init"))
      py_env.globals().get("worker_node_init")();

    py_env.globals().get("worker_node_main")();
  } catch (boost::python::error_already_set) {
    PyErr_Print();
    throw;
  }

  return 0;
}

