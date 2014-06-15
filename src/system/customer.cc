#include "system/customer.h"

namespace PS {

Customer::Customer(): sys_(Postoffice::instance()), exec_(*this) {
  // exec_thread_ = new std::thread(&Executor::run, &exec_);
  exec_thread_ = unique_ptr<std::thread>(new std::thread(&Executor::run, &exec_));
}

Customer::~Customer() {
  exec_.stop();
  exec_thread_->join();
}

} // namespace PS
