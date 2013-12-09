#include "gtest/gtest.h"
#include "util/common.h"
#include "system/postmaster.h"

using namespace PS;
int my_rank = 0;

TEST(Postmaster, init_ping) {
  pid_t pid = fork();
  if (pid == 0) {
    my_rank ++;
    pid_t pid2 = fork();
    if (pid2 == 0)
      my_rank ++;
  }
  
  Postmaster *pm = Postmaster::Instance();
  FLAGS_my_rank = my_rank;
  pm->Init();

  if (FLAGS_my_rank == 0)
    pm->send_cmd();
  else
    pm->receive_cmd();
}
