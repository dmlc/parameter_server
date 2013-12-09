#pragma once

#include "util/common.h"
#include "util/rawarray.h"
#include "proto/header.pb.h"

namespace PS {

class Mail {
 public:
  Mail() { }
  Mail(const Header& flag,
       const RawArray& keys = RawArray(),
       const RawArray& vals = RawArray()) :
      flag_(flag), keys_(keys), vals_(vals) { }
  // accessors
  Header& flag() { return flag_; }
  const Header& flag() const { return flag_; }
  RawArray& keys() { return keys_; }
  const RawArray& keys() const { return keys_; }
  RawArray& vals() { return vals_; }
  const RawArray& vals() const { return vals_; }
  // mutators
  void set_keys(const RawArray& keys) { keys_ = keys; }
  void set_vals(const RawArray& vals) { vals_ = vals; }
  // return an empty reply mail with the proper flag
  Mail Reply() const {
    Header head = flag_;
    head.set_type(Header::REPLY);
    head.set_recver(head.sender());
    CHECK(head.has_pull());
    head.set_allocated_push(head.release_pull());
    CHECK(!head.has_pull());
    return Mail(head, keys_);
  }
  bool Serialization(std::string *data);
  char *ParseFromString(char *data, char *end_of_data);
 private:
  Header flag_;
  RawArray keys_;
  RawArray vals_;
};


} // namespace PS
