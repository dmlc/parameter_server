#pragma once

#include "util/common.h"

#include "system/postmaster.h"
#include "system/postoffice.h"

namespace PS {

// An object shared by several nodes. This object is identified by an unique
// name (string). To reduce the communication cost, this object will be assigned
// a unique id (int) during initialization.
// TODO if it is an running instance (inference), assure there is no other
// instance has the same name. it if is a container, there are two options:
// create a new container, or connect to an existing one.
class SharedObj {
 public:
  static const int kInvalidId = -1;
  SharedObj() { }
  SharedObj(const string& name) :
      name_(name), id_(kInvalidId), obj_inited_(false) { }
  // initialization : get assigned id_ from the master
  virtual void Init();
  // wait until initialization success
  virtual void WaitInited();
  // accessors
  const string& name() { return name_; }
  int32 id() { return id_; }

  // query about my node
  Node& my_node() { return postmaster_->addr_book()->my_node(); }

  // the short name, for debug use
  string SName() { return StrCat(my_node().ShortName(), ": "); }

 protected:
  string name_;
  int32 id_;
  Postoffice *postoffice_;
  Postmaster *postmaster_;
  bool obj_inited_;
 private:
  DISALLOW_COPY_AND_ASSIGN(SharedObj);
};
} // namespace PS
