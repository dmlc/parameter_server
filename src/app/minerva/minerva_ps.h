#pragma once
#include "shared_model.h"
#include <string>

namespace minerva {
namespace basic {
void PushGradAndPullWeight(const float * grad, float * weights, size_t size,
                           const std::string & layer_name);

static PS::SharedModel<float> *shared_model;
}
}
