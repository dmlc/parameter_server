#pragma once
#include "cxxnet/proto/cxxnet.pb.h"
#include "system/app.h"
namespace PS {
namespace CXXNET {

// the interface to PS system, see src/main.cc
App* createApp(const std::string& name, const Config& conf);

} // namespace CXXNET
} // namespace PS
