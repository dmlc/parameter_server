#include "gtest/gtest.h"
// #include "util/matrix.h"
using namespace PS;
TEST(Matrix, LoadInfo) {


}

TEST(Matrix, WriteInfo) {
  MatrixInfo info;

  info.set_row_start(1);
  info.set_row_end(100);
  info.set_col_end(8);
  info.set_col_start(900);

  string str;
  TextFormat::PrintToString(info, &str);
  LL << str;
}
