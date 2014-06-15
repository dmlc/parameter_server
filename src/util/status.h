// A Status encapsulates the result of an operation.  It may indicate success,
// or it may indicate an error with an associated error message.
//
// Multiple threads can invoke const methods on a Status without
// external synchronization, but if any of the threads may call a
// non-const method, all threads accessing the same Status must use
// external synchronization.
#pragma once
#include "util/common.h"

namespace PS {

// convert a non-string to string
template <class T>
static string strfy(const T& t) {
  std::ostringstream os;
  if(!(os << t)) {
    // TODO Handle the Error
    std::cerr << "[Error] strfy error!!!" << std::endl;
    exit(1);
  }
  return os.str();
}

class Status {
 public:
  // Create a success status.
  Status() : state_(NULL) { }
  ~Status() { delete state_; }

  // Copy the specified status.
  Status(const Status& s);
  void operator=(const Status& s);

  // Return a success status.
  static Status OK() { return Status(); }

  // Return error status of an appropriate type.
  static Status NotFound(const string& msg) {
    return Status(kNotFound, msg);
  }
  static Status NotSupported(const string& msg) {
    return Status(kNotSupported, msg);
  }
  static Status InvalidArgument(const string& msg) {
    return Status(kInvalidArgument, msg);
  }
  static Status IOError(const string& msg) {
    return Status(kIOError, msg);
  }
  static Status NetError(const string& msg) {
    return Status(kNetError, msg);
  }

  // Returns true iff the status indicates success.
  bool ok() const { return (state_ == NULL); }

  // Return a string representation of this status suitable for printing.
  // Returns the string "OK" for success.
  string ToString() const {
    if(state_ == NULL) {
      return "OK";
    } else  {
      string err_code = "Error code: " + strfy(state_->first);
      string err_msg = "Error msg: " + strfy(state_->second);
      return err_code + "\t" + err_msg;
    }
  }

 private:
  enum Code {
    kOk = 0,
    kNotFound = 1,
    kCorruption = 2,
    kNotSupported = 3,
    kInvalidArgument = 4,
    kIOError = 5,
    kNetError = 6,
  };
  Code code() const { return (state_ == NULL) ? kOk : state_->first; }

  Status(Code code, const string& msg) {
    CHECK_NE(code, kOk);
    state_ = new std::pair<Code, string>(code, msg);
  }

  typedef std::pair<Code, string> State;
  State* state_;
};
} // namespace PS
