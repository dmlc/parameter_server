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


  virtual void encode(const MessagePtr& msg) { }
  virtual void decode(const MessagePtr& msg) { }

  static FilterConfig* find(FilterConfig::Type type, const MessagePtr& msg) {
    return find(type, &(msg->task));
  }
  static FilterConfig* find(FilterConfig::Type type, Task* task);
};

} // namespace
