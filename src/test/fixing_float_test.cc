#include "gtest/gtest.h"
#include "filter/fixing_float.h"
using namespace PS;
TEST(FIXING_FLOAT, EncodeDecode) {
  MessagePtr msg(new Message());
  auto conf = msg->addFilter(FilterConfig::FIXING_FLOAT)->mutable_fixed_point();
  conf->set_min_value(-90);
  conf->set_max_value(90);
  conf->set_num_bytes(3);

  SArray<float> ax = {100.0, .1, -100.0}; msg->addValue(ax);
  SArray<double> bx = {100.0, .1, -100.0}; msg->addValue(bx);


  FixingFloatFilter filter;
  filter.encode(msg);
  filter.decode(msg);

  LL << SArray<float>(msg->value[0]);
  LL << SArray<double>(msg->value[1]);
}


TEST(FIXING_FLOAT, Error) {
}
