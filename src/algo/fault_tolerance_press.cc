#include <stdlib.h>
#include <vector>
#include <time.h>
#include <chrono>
#include <gflags/gflags.h>
#include "box/vectors.h"
#include "util/xarray.h"
#include "util/key.h"

namespace PS {

DECLARE_int32(my_rank);
DECLARE_string(my_type);
DECLARE_int32(num_client);
DECLARE_int32(num_server);

DECLARE_int32(global_feature_num);
DECLARE_int32(local_feature_num);


void Server();
void Client();

using std::vector;
using std::string;


vector<int> GenGlobalFeaId();

void Server() {
  vector<int> global_id = GenGlobalFeaId();
  XArray<Key> keys(FLAGS_global_feature_num);
  for (size_t i = 0; i < global_id.size(); ++i) {
    keys.set(i, global_id[i]);
  }
  Vectors<double> vec("ft", FLAGS_global_feature_num, 1);
  vec.SetAggregator(NodeGroup::kClients);
  while (1) {
    std::this_thread::sleep_for(seconds(1));
    LOG(WARNING) << "Vec[0]" << vec.Vec(0)[0];
  }
}

vector<int> GenLocalFeaId();

void Client() {
  // generate local feature
  vector<int> local_id = GenLocalFeaId();

  XArray<Key> keys(FLAGS_local_feature_num);
  for (size_t i = 0; i < local_id.size(); ++i) {
    keys.set(i, local_id[i]);
  }
  Vectors<double> vec("ft", FLAGS_global_feature_num, 1, keys);
  vec.SetMaxDelay(kint32max,kint32max);

  while(1) {
    for (int i = 0; i < FLAGS_local_feature_num; ++i) {
      vec.Vec(0)[i] = vec.Vec(0)[i] + 1;
    }
    // LOG(WARNING) << "push to server.";
    vec.PushPull(KeyRange::All(), {0}, kValue, {0}, kDelta);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

vector<int> GenLocalFeaId() {
  srand(time(NULL));
  CHECK(FLAGS_global_feature_num >= FLAGS_local_feature_num)
                                 << "global fea num should be larger than local fea num";
  vector<int> feature_id;
  for (int i = 0; i < FLAGS_global_feature_num; ++i) {
    feature_id.push_back(i);
  }

  std::random_shuffle(feature_id.begin(), feature_id.end());
  feature_id.resize(FLAGS_local_feature_num);

  sort(feature_id.begin(), feature_id.end());
  return feature_id;
}

vector<int> GenGlobalFeaId() {
  CHECK(FLAGS_global_feature_num >= FLAGS_local_feature_num)
                                 << "global fea num should be larger than local fea num";
  vector<int> feature_id;
  for (int i = 0; i < FLAGS_global_feature_num; ++i) {
    feature_id.push_back(i);
  }
  return feature_id;
}

}

int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  if (PS::FLAGS_my_type == "client") {
    PS::Client();
  } else {
    PS::Server();
  }
  return 0;
}
