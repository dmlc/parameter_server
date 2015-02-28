#include <glog/logging.h>
#include "ps.h"
#include "minerva_ps.h"

template <typename V>
inline std::string arrstr(const V* data, int n) {
  std::stringstream ss;
  ss << "[" << n << "]: ";
  for (int i = 0; i < n; ++i) ss << data[i] << " ";
  return ss.str();
}

void InitLayer(const std::string & name, float * data, size_t size) {
  for(size_t i = 0; i < size; i++)
    data[i] = 0;
}

void UpdateLayer(const std::string & name, float * weight, float * grad, size_t size) {
  float eta = .1;
  for(size_t i = 0; i < size; i++)
    weight[i] -= eta * grad[i];
}

int MinervaWorkerMain(int rank, int size, int argc, char ** argv)
{
  using minerva::PushGradAndPullWeight;
  const int n = 10;
  float grad[10];
  float weight[10];

  PushGradAndPullWeight(nullptr, weight, n, "layer0");
  LOG(ERROR) << "worker " << PS::MyRank() << "/" << PS::RankSize()
             << " init weight " << arrstr(weight, n);

  for (int j = 1; j < 4; ++j) {
    for (int i = 0; i < n; ++i) grad[i] = j;
    PushGradAndPullWeight(grad, weight, n, "layer0");
    LOG(ERROR) << "worker " << PS::MyRank() << "/" << PS::RankSize()
               << " pull weight " << arrstr(weight, n);
  }

  return 0;
}
