#include "gtest/gtest.h"
#include "system/replica_manager.h"
#include "util/xarray.h"
using namespace PS;

TEST(ReadFromDisk, Replica) {
  ReplicaManager *rm = ReplicaManager::Instance();
  rm->Init();
  ReplicaManager::ReplicaInfo info;
  info.container_name = "ft";
  info.node_id = 0;
  info.replica_id = 2;
  std::string dir = "./algo/";
  //rm->ReadFromDisk(dir, info);
}
