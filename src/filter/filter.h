#pragma once
#include "system/message.h"
#include "filter/proto/filter.pb.h"
#include "util/shared_array_inl.h"

namespace PS {

// A filter should be thread safe
class Filter {
 public:
  Filter() { }
  virtual ~Filter() { }

  static Filter* create(const FilterConfig& conf);


  virtual void encode(Message* msg) { }
  virtual void decode(Message* msg) { }

  static FilterConfig* find(FilterConfig::Type type, Message* msg) {
    return find(type, &(msg->task));
  }
  static FilterConfig* find(FilterConfig::Type type, Task* task);
};

} // namespace
