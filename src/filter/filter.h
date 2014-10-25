#pragma once
#include "system/message.h"
#include "proto/filter.pb.h"
#include "base/shared_array_inl.h"

namespace PS {

class Filter;
typedef std::shared_ptr<Filter> FilterPtr;

// A filter should be thread safe
class Filter {
 public:
  static FilterPtr create(const FilterConfig& conf);

  virtual void encode(const MessagePtr& msg) { }
  virtual void decode(const MessagePtr& msg) { }

  static FilterConfig* find(FilterConfig::Type type, const MessagePtr& msg) {
    return find(type, &(msg->task));
  }
  static FilterConfig* find(FilterConfig::Type type, Task* task);
};

} // namespace
