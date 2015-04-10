#pragma once
#include "system/message.h"
#include "proto/filter.pb.h"
#include "util/shared_array_inl.h"

namespace ps {

// A filter should be thread safe
class IFilter {
 public:
  IFilter() { }
  virtual ~IFilter() { }

  static IFilter* create(const Filter& conf);


  virtual void Encode(Message* msg) { }
  virtual void Decode(Message* msg) { }

  static Filter* find(Filter::Type type, Message* msg) {
    return find(type, &(msg->task));
  }
  static Filter* find(Filter::Type type, Task* task);
};

} // namespace
