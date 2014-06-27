#include <mpi.h>
#include "system/postoffice.h"
#include "app/app.h"
#include "util/local_machine.h"

DEFINE_string(interface, "lo0", "network interface will use");
DEFINE_int32(port, 8000, "network port");

namespace PS {

void Init() {
  int my_rank, rank_size;
  CHECK(!MPI_Comm_rank(MPI_COMM_WORLD, &my_rank));
  CHECK(!MPI_Comm_size(MPI_COMM_WORLD, &rank_size));

  // get my node
  int nserver = FLAGS_num_servers;
  int nclient = FLAGS_num_workers;
  if (my_rank == 0) CHECK_GT(rank_size, nserver + nclient);

  FLAGS_num_unused = rank_size - nserver - nclient - 1;

  auto ip = LocalMachine::IP(FLAGS_interface);
  CHECK(!ip.empty()) << "fail to get the ip from interface " << FLAGS_interface;

  string my_node = "role:";
  string id;
  if (my_rank == 0) {
    my_node += "SCHEDULER";
    id = "H";
  } else if (my_rank < nclient + 1) {
    my_node += "CLIENT";
    id = "W" + std::to_string(my_rank - 1);
  } else if (my_rank < nclient + nserver + 1) {
    my_node += "SERVER";
    id = "S" + std::to_string(my_rank - nclient - 1);
  } else {
    my_node += "UNUSED";
    id = "U" + std::to_string(my_rank - nclient - nserver - 1);
  }
  my_node += ",hostname:'" + ip + "',port:" +
             std::to_string(FLAGS_port+my_rank) + ",id:'" + id + "'";
  FLAGS_my_node = my_node;

  // send my node to the scheduler, and save into ../config/nodes
  if (my_rank == 0) {
    std::ofstream file(FLAGS_node_file);
    CHECK(file.good()) << " fail to write " + FLAGS_node_file;
    file << my_node << "\n";
    char node[100];
    MPI_Status stat;
    for (int i = 1; i < rank_size; ++i) {
      memset(node, 0, 100);
      CHECK(!MPI_Recv(node, 100, MPI_CHAR, i, 0, MPI_COMM_WORLD, &stat));
      file << node << "\n";
    }
  } else {
    int n = my_node.size();
    char send[n+5];
    memcpy(send, my_node.data(), n);
    CHECK(!MPI_Send(send, n, MPI_CHAR, 0, 0, MPI_COMM_WORLD));
  }
}

} // namespace PS

int main(int argc, char *argv[]) {
  FLAGS_logtostderr = 1;
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  CHECK(!MPI_Init(&argc, &argv));

  try {
    PS::Init();
    PS::Postoffice::instance().run();
  } catch (std::exception& e) {
    LL << e.what();
  }


  // LL << "ok";
  // MPI_Barrier(MPI_COMM_WORLD);
  // LL << "done";
  CHECK(!MPI_Finalize());
  // try {
  // }  catch (std::bad_alloc& ba) {
  //   std::cerr << "bad_alloc caught: " << ba.what() << '\n';
  // }

  return 0;
}
