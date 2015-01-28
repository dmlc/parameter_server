#include <iostream>
#include <map>
#include <string>
#include "ps.h"
#include "parameter/kv_vector.h"
using namespace PS;

typedef KVVector<uint64, double> SyncVector;
std::map<std::string, SyncVector *> model_;

void AddLayer(const std::string & layerName, size_t nParams) {
  auto layer = new SyncVector(layerName);
  layer->key().resize(nParams);
  for (size_t i = 0; i < nParams; i++)
    layer->key()[i] = i;
  layer->value().resize(nParams);
  CHECK(model_.insert(make_pair(layerName, layer)).second) << "layer " << layerName << " already exists!";
}

SyncVector * GetLayer(const std::string & layerName)
{
  auto it = model_.find(layerName);
  CHECK(it != model_.end()) << "trying to pull " << layerName << " without declaring it first";
  return it->second;
}

void PullLayer(const std::string & layerName, double * grad, double * weight) {
  auto layer = GetLayer(layerName);
  MessagePtr msg(new Message(kServerGroup));
  
  size_t nParams = layer->value().size();

  memcpy(weight, grad, sizeof(double)*nParams);
  layer->value().reset(weight, nParams, false);
  msg->key = layer->key();  
  int pull_time = layer->pull(msg);

  layer->waitOutMsg(kServerGroup, pull_time);
}

void DeclareLayers()
{
  AddLayer("w", 6);
  AddLayer("w2", 6);
}

namespace PS {

class MinervaServer : public App {
 public:
  MinervaServer() : App() { }
  virtual ~MinervaServer() { }

  void init() {
    LL << myNodeID() << ", this is server " << myRank();
    DeclareLayers();
    auto & v = GetLayer("w")->value();
    for (size_t i = 0; i < v.size(); i++)
      v[i] = (double)i / 10;
  }
};

App* CreateServerNode(const std::string& conf) {
  return new MinervaServer();
}
} // namespace PS

std::ostream & operator << (std::ostream & os, const std::vector<double> & v)
{
  for (auto & d : v)\
    os << " " << d;
  return os;
}

int WorkerNodeMain(int argc, char *argv[]) {
  using namespace PS;
  LOG(ERROR) << MyNodeID() <<  ": this is worker " << MyRank();

  DeclareLayers();
  std::vector<double> grad(6, 2);
  std::vector<double> weight(6, 0);
  PullLayer("w", &grad[0], &weight[0]);
  LOG(ERROR) << MyNodeID() << ": " << weight;
  
  return 0;
}
