#pragma once
#include "python_env.h"
#include "python_updater.h"
#include "shared_model.h"

namespace PS {

class PythonServer : public App {
public:
  PythonServer(const std::string& script, const std::string& conf) : App(), script_(script), conf_(conf) {
    updater_ = new PythonUpdater<float>(&py_env_);
    shared_model_ = new SharedModel<float>();
    shared_model_->setUpdater(updater_);
  }

  virtual void init() {
    reset(script_, conf_);
  }

  void reset(const std::string& script, const std::string& conf) {
    script_ = script;
    conf_ = conf;

    int argc = 2;
    argv_[0] = const_cast<char*>(script_.c_str());
    argv_[1] = const_cast<char*>(conf_.c_str());

    py_env_.load_file(script_.c_str(), argc, argv_);

    try {
      if (py_env_.globals().has_key("server_node_init"))
        py_env_.globals().get("server_node_init")();
    } catch (boost::python::error_already_set) {
      PyErr_Print();
      throw;
    }
  }

  virtual ~PythonServer() {
    delete updater_;
    delete shared_model_;
  }

private:
  std::string script_;
  std::string conf_;

  char* argv_[2];

  PythonEnv py_env_;
  Updater<float> *updater_;
  SharedModel<float> *shared_model_;
};

} // namespace PS

