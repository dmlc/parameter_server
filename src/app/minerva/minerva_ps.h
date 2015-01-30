#pragma once
#include <string>

void InitLayer(const std::string & name, float * data, size_t size);
void UpdateLayer(const std::string & name, float * weight, float * grad, size_t size);
int MinervaWorkerMain(int rank, int size, int argc, char ** argv);

namespace minerva {
namespace basic {
void PushGradAndPullWeight(const float * grad, float * weights, size_t size,
                           const std::string & layer_name);

}
}
