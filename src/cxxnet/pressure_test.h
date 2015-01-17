#include "cxxnet/cxxnet_node.h"
namespace PS {
namespace CXXNET {
//

class PressureScheduler : public CXXNetScheduler {
 public:
  PressureScheduler(const string& name, const Config& conf)
      : CXXNetScheduler(name, conf) { }
  virtual ~PressureScheduler() { }
 protected:
  virtual void showProgress() {

  }

  virtual void addProgress(const NodeID& sender, const Progress& prog) {

  }
};


class PressureServer : public CXXNetServer {
 public:
  PressureServer(const string& name, const Config& conf)
      : CXXNetServer(name, conf) { }
  virtual ~PressureServer() { }
 protected:
  virtual updateModel() {

  }
};


class PressureWorker : public CXXNetWorker {
 public:
  PressureWorker(const string& name, const Config& conf)
      : CXXNetWorker(name, conf) { }
  virtual ~PressureWorker() { }
 protected:

};

} // namespace CXXNET
} // namespace PS
