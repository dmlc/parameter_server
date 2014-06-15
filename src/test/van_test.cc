#include "gtest/gtest.h"
#include "util/common.h"
#include "system/van.h"
#include "system/message.h"
#include <sys/types.h>
#include <unistd.h>

int my_rank = 0;
int rank_size = 3;
std::vector<Node> nodes;

using namespace PS;

// send by ring
Message Ring(Message orig, Van *van) {
  auto send = orig;
  for (int i = 0; i < rank_size; ++i) {
    int to = (my_rank + 1) % rank_size;
    // int from = (my_rank - 1 + rank_size) % rank_size;
    send.recver = nodes[to].uid();
    send.sender = nodes[my_rank].uid();
    auto s = van->send(send);
    EXPECT_TRUE(s.ok()) << s.ToString();
    Message recv;
    EXPECT_TRUE(van->recv(&recv).ok());
    // EXPECT_EQ(recv.label().sender(), from);
    send = recv;
  }
  return send;
}

TEST(Van, Message) {
  pid_t pid = fork();
  if (pid == 0) {
    my_rank ++;
    pid_t pid2 = fork();
    if (pid2 == 0)
      my_rank ++;
  }

  std::vector<string> local = {
    "role:CLIENT,hostname:'localhost',port:7000",
    "role:CLIENT,hostname:'localhost',port:7001",
    "role:CLIENT,hostname:'localhost',port:7002",
    "role:CLIENT,hostname:'localhost',port:7003",
    "role:CLIENT,hostname:'localhost',port:7004"
  };
  for (int i = 0; i < rank_size; ++i) {
    nodes.push_back(Van::parseNode(local[i]));
  }
  FLAGS_my_node = local[my_rank];
  Van van;
  van.init();
  for (int i = 0; i < rank_size; ++i) {
    if (i != my_rank) {
      EXPECT_TRUE(van.connect(nodes[i]).ok());
    }
  }



//   std::vector<int> a = {my_rank * 10, my_rank * 20};
//   SArray<int> sa; sa.copyFrom(a.begin(), a.end());
//   std::vector<double> b = {my_rank * .1, my_rank * .2};
//   SArray<double> sb; sb.copyFrom(b.begin(), b.end());

  Message p1;
//   PackedTask task;
//   Message p1(task);
//   Message p2(task); p2.add(sa);
//   Message p3(task); p3.add(sa); p3.add(sb);

  Message recv;
  recv = Ring(p1, &van);
//   EXPECT_TRUE(recv.body().empty());

//   recv = Ring(p2, &van);
//   EXPECT_TRUE(recv.body()[0] == sa);

//   recv = Ring(p3, &van);
//   EXPECT_TRUE(recv.body()[0] == sa);
//   EXPECT_TRUE(recv.body()[1] == sb);

  usleep(100);
  van.destroy();
  int ret;
  wait(&ret);
}
