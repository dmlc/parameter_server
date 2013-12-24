#include "gtest/gtest.h"
#include "system/shared_obj.h"
#include "system/node.h"

using namespace PS;
TEST(SharedObj, GetID) {
  FORK2C2S;

  if (Node::Client(FLAGS_my_type)) {
    SharedObj o1("o1");
    o1.Init();
    LL << o1.id();
  } else {
    sleep(2);
  }
  WAIT;
}
