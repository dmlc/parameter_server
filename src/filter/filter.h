#pragma once
#include "system/message.h"
#include "proto/filter.pb.h"

namespace PS {

class Filter;
typedef std::shared_ptr<Filter> FilterPtr;

class Filter {
 public:
  static FilterPtr create(const FilterConfig& conf) {

  }
  virtual void encode(const MessagePtr& msg) { }
  virtual void decode(const MessagePtr& msg) { }

  FilterConfig* find(FilterConfig::Type type, const MessagePtr& msg) {
    return find(type, &(msg->task));
  }

  FilterConfig* find(FilterConfig::Type type, Task* task) {
    for (int i = 0; i < task->filter_size(); ++i) {
      if (task->filter(i).type() == type) return task->mutable_filter(i);
    }
    return nullptr;
  }
};

} // namespace
