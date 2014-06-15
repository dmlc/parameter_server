// #include "gtest/gtest.h"
// #include "util/eigen3.h"

// TEST(EIGEN3, LOADXY) {
//   using namespace PS;
//   DSMat X_;
//   DVec Y_;
//   auto t = tic();
//   LoadXY("../data/rcv1.train", 0, 5000, &Y_, &X_);
//   LL << toc(t);

//   DSMat data_;
//   DVec label_;

//   LoadXY("../data/smalldata", 0, 10, &label_, &data_);

//   LL << data_;

//   CHECK_EQ(data_.coeffRef(4, 2), -29);
//   CHECK_EQ(data_.coeffRef(6, 4), -26);
//   CHECK_EQ(data_.coeffRef(1, 0), 46);
//   CHECK_EQ(data_.coeffRef(8, 1), 33);
//   CHECK_EQ(data_.coeffRef(1, 4), -13);
//   CHECK_EQ(data_.coeffRef(0, 0), 0);
//   CHECK_EQ(data_.coeffRef(9, 4), -48);

//   // TODO test the correctness
// }
