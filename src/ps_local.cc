// #include "app/gradient_descent/gd.h"

#include "system/postoffice.h"
#include "app/app.h"

DEFINE_int32(port, 8000, "network port");

namespace PS {

// fock threads
void Init() {
  std::vector<string> nodes;

  {
  int port = FLAGS_port;
  std::ofstream file(FLAGS_node_file);
  CHECK(file.good()) << " fail to write " + FLAGS_node_file;
  FLAGS_my_node = "role:SCHEDULER, hostname:'localhost', port:"
                  + std::to_string(port++) + ", uid:'H'";
  file << FLAGS_my_node << "\n";

  for (int i = 0; i < FLAGS_num_workers; ++i) {
    string node = "role:CLIENT, hostname:'localhost', port:"
                  + std::to_string(port++) + ", uid:'C" + std::to_string(i) + "'";
    nodes.push_back(node);
    file << node << "\n";
  }
  for (int i = 0; i < FLAGS_num_servers; ++i) {
    string node = "role:SERVER, hostname:'localhost', port:"
                  + std::to_string(port++) + ", uid:'S" + std::to_string(i) + "'";
    nodes.push_back(node);
    file << node << "\n";
  }
  file.close();
  }

  for (int i = 0; i < FLAGS_num_workers + FLAGS_num_servers; ++i) {
    if (fork() == 0)
      FLAGS_my_node = nodes[i];
    else
      break;
  }
}

} // namespace PS

int main(int argc, char *argv[]) {
  FLAGS_logtostderr = 1;
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  PS::Init();
  PS::Postoffice::instance().run();

  usleep(200);
  // WaitProcess();
  // int ret; wait(&ret);
  // LL << "done";
  wait();
  return 0;
}
