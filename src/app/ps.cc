#include "app/gradient_descent/gd.h"

DEFINE_string(algorithm, "sgd", "algorithm");

int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  using namespace PS;

  // FLAGS_my_type = "c";
  // FLAGS_num_client = 2;
  // FLAGS_num_server = 2;
  // pid_t pid = fork();
  // if (pid == 0) {
  //   FLAGS_my_type = "s";
  //   pid_t pid2 = fork();
  //   if (pid2 == 0) {
  //     FLAGS_my_rank ++;
  //     FLAGS_my_type = "s";
  //     pid_t pid3 = fork();
  //     if (pid3 == 0)
  //       FLAGS_my_type = "c";
  //   }
  // }

  Inference *ifr = NULL;
  // if (FLAGS_algorithm == "sgd")
    ifr = new GD("gradient_descent");

  ifr->Init();
  ifr->Run();

  // int ret;
  // wait(&ret);

  LL << "exit";
  return 0;
}
