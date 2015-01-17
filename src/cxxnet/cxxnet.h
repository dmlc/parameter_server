#pragram once
#include "cxxnet/proto/cxxnet.pb.h"
namespace PS {
namespace CXXNET {

// the interface to PS system, see src/main.cc
class App;
App* createApp(const string& name, const CXXNetConfig& conf);

} // namespace CXXNET
} // namespace PS
