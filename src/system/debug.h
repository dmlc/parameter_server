#include "util/common.h"
#include "system/postoffice.h"

namespace PS {

static std::vector<string> local_clients = {
  "role:CLIENT,hostname:'localhost',port:7000",
  "role:CLIENT,hostname:'localhost',port:7001",
  "role:CLIENT,hostname:'localhost',port:7002",
  "role:CLIENT,hostname:'localhost',port:7003",
  "role:CLIENT,hostname:'localhost',port:7004",
  "role:CLIENT,hostname:'localhost',port:7005",
  "role:CLIENT,hostname:'localhost',port:7006",
  "role:CLIENT,hostname:'localhost',port:7007",
  "role:CLIENT,hostname:'localhost',port:7008",
  "role:CLIENT,hostname:'localhost',port:7009",
  "role:CLIENT,hostname:'localhost',port:7010",
  "role:CLIENT,hostname:'localhost',port:7011"
};

static std::vector<string> local_servers = {
  "role:SERVER,hostname:'localhost',port:8000",
  "role:SERVER,hostname:'localhost',port:8001",
  "role:SERVER,hostname:'localhost',port:8002",
  "role:SERVER,hostname:'localhost',port:8003",
  "role:SERVER,hostname:'localhost',port:8004",
  "role:SERVER,hostname:'localhost',port:8005",
  "role:SERVER,hostname:'localhost',port:8006",
  "role:SERVER,hostname:'localhost',port:8007",
  "role:SERVER,hostname:'localhost',port:8008",
  "role:SERVER,hostname:'localhost',port:8009",
  "role:SERVER,hostname:'localhost',port:8010",
  "role:SERVER,hostname:'localhost',port:8011"
};

static void ForkProcess() {
  std::vector<string> nodes;
  for (int i = 0; i < FLAGS_num_workers; ++i)
    nodes.push_back(local_clients[i]);
  for (int i = 0; i < FLAGS_num_servers; ++i)
    nodes.push_back(local_servers[i]);
  for (int i = 0; i < FLAGS_num_workers + FLAGS_num_servers; ++i) {
    if (fork() == 0)
      FLAGS_my_node = nodes[i];
    else
      break;
  }
}

static void WaitProcess() {
  // int ret;
  // wait(&ret);
}

} // namespace PS
