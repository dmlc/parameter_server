#include "algo/graddesc.h"

DEFINE_string(algorithm, "sgd", "algorithm");

int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  using namespace PS;

  FLAGS_my_type = "c";
  FLAGS_num_client = 1;
  pid_t pid = fork();
  if (pid == 0) {
    FLAGS_my_type = "s";
  }

  //   FLAGS_my_type = "client";
  //   pid_t pid2 = fork();
  //   if (pid2 == 0) {
  //     FLAGS_my_rank = 1;
  //   }
  // }

  Inference *algo = NULL;
  // if (FLAGS_algorithm == "sgd")
    algo = new GD("gradient_descent");

  algo->Init();
  algo->Run();

  LL << "exit";
  return 0;
}
