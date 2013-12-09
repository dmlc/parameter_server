#include "gtest/gtest.h"
#include "util/common.h"
#include "util/xarray.h"
#include "system/van.h"
#include <sys/types.h>
#include <unistd.h>

int my_rank = 0;
int rank_size = 3;

using namespace PS;

// Mail Ring(Mail orig, Van *van) {
//   Mail send = orig;
//   for (int i = 0; i < rank_size; ++i) {
//     int to = (my_rank + 1) % rank_size;
//     int from = (my_rank - 1 + rank_size) % rank_size;
//     send.flag().set_recver(to*2);
//     send.flag().set_sender(my_rank);
//     van->Send(send);
//     Mail recv;
//     van->Recv(&recv);
//     EXPECT_EQ(recv.flag().sender(), from);
//     send = recv;
//   }
//   return send;
// }

// TEST(Van, KEY_VALUE) {
//   pid_t pid = fork();
//   if (pid == 0) {
//     my_rank ++;
//     pid_t pid2 = fork();
//     if (pid2 == 0)
//       my_rank ++;
//   }
//   Van *van = Van::Instance();
//   EXPECT_TRUE(van->Init());

//   std::vector<string> addr =
//       split("tcp://localhost:7000,tcp://localhost:7001,tcp://localhost:7002", ',');

//   for (int i = 0; i < rank_size; ++i) {
//     Node node(Node::kTypeServer, i, addr[i]);
//     if (i == my_rank) {
//       EXPECT_TRUE(van->Bind(node));
//     } else {
//       EXPECT_TRUE(van->Connect(node));
//     }
//   }

//   Header flag;
//   flag.set_type(Header::PUSH);
//   flag.set_name(StrCat(my_rank));
//   flag.set_time(38);
//   flag.set_key_start(100);
//   flag.set_key_end(210);
//   flag.set_key_cksum(345);

//   XArray<int64> k(3);
//   k[0] = my_rank * 10 + 0;
//   k[1] = my_rank * 10 + 1;
//   k[2] = my_rank * 10 + 2;

//   XArray<double> v(3);
//   v[0] = my_rank * 10 + 0.5;
//   v[1] = my_rank * 10 + 1.5;
//   v[2] = my_rank * 10 + 2.5;


//   Mail recv;
//   // LL << send.flag().DebugString();

//   flag.set_key(false);
//   flag.set_value(true);
//   Mail orig1(flag, k.raw(), v.raw());
//   recv = Ring(orig1, van);
//   EXPECT_FALSE(recv.keys() == k.raw());
//   EXPECT_TRUE(recv.vals() == v.raw());


//   flag.set_key(true);
//   flag.set_value(false);
//   Mail orig2(flag, k.raw(), v.raw());
//   recv = Ring(orig2, van);
//   EXPECT_TRUE(recv.keys() == k.raw());
//   EXPECT_FALSE(recv.vals() == v.raw());

//   flag.set_key(true);
//   flag.set_value(true);
//   Mail orig3(flag, k.raw(), v.raw());
//   recv = Ring(orig3, van);
//   EXPECT_TRUE(recv.keys() == k.raw());
//   EXPECT_TRUE(recv.vals() == v.raw());


//   flag.set_key(false);
//   flag.set_value(false);
//   Mail orig4(flag, k.raw(), v.raw());
//   recv = Ring(orig4, van);
//   EXPECT_FALSE(recv.keys() == k.raw());
//   EXPECT_FALSE(recv.vals() == v.raw());
//   std::this_thread::sleep_for(milliseconds(100));
//   // sleep(1);
//   // FIXME destroy has problems!
//   // EXPECT_TRUE(van->Destroy());
//   int ret;
//   wait(&ret);
// }

TEST(Van, NodeManagementInfo) {
  pid_t pid = fork();
  if (pid == 0) {
    my_rank ++;
    pid_t pid2 = fork();
    if (pid2 == 0)
      my_rank ++;
  }
  std::vector<string> addr =
      split("tcp://localhost:7000,tcp://localhost:7001,tcp://localhost:7002", ',');
  
  Van *van = new Van();
  van->Init();

  for (int i = 0; i < rank_size; ++i) {
    Node node(Node::kTypeServer, i, "xxx",  addr[i]);
    if (i == my_rank) {
      EXPECT_TRUE(van->Bind(node, 1));
    } else {
      EXPECT_TRUE(van->Connect(node, 1));
    }
  }

  NodeManagementInfo mgt_info;
  if (my_rank == 0)
  {
    mgt_info.set_command_id(NodeManagementInfo::CLIENT_MSG_KEY_RANGE);
    
    MsgKeyRange* mkr = mgt_info.add_msg_key_ranges();
    mkr->set_node_id(1);
    mkr->set_key_start(0);
    mkr->set_key_end(100);
    
    mkr = mgt_info.add_msg_key_ranges();
    mkr->set_node_id(2);
    mkr->set_key_start(101);
    mkr->set_key_end(200);

    mgt_info.set_sender(my_rank);
    
    mgt_info.set_recver((my_rank + 1)*2);
    van->Send(mgt_info);

    mgt_info.set_recver((my_rank + 2)*2);
    van->Send(mgt_info);
    LL << mgt_info.command_id();
    sleep(1);
  }
  else
  {
    van->Recv(&mgt_info);
    string mgt_str;
    mgt_info.SerializeToString(&mgt_str);
    EXPECT_TRUE(mgt_info.command_id() == NodeManagementInfo::CLIENT_MSG_KEY_RANGE);
    MsgKeyRange mkr1 = mgt_info.msg_key_ranges(0);
    MsgKeyRange mkr2= mgt_info.msg_key_ranges(1);
    EXPECT_TRUE(mkr1.node_id() == 1);
    EXPECT_TRUE(mkr1.key_start() == 0);
    EXPECT_TRUE(mkr1.key_end() == 100);
    EXPECT_TRUE(mkr2.node_id() == 2);
    EXPECT_TRUE(mkr2.key_start() == 101);
    EXPECT_TRUE(mkr2.key_end() == 200);
  }
}
