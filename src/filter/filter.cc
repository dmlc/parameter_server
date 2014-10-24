#include "filter/filter.h"
#include "filter/compressing.h"
#include "filter/key_caching.h"

namespace PS {

FilterPtr Filter::create(const FilterConfig& conf) {
  switch (conf.type()) {
    case FilterConfig::KEY_CACHING:
      return FilterPtr(new KeyCachingFilter());
    case FilterConfig::COMPRESSING:
      return FilterPtr(new CompressingFilter());
    default:
      CHECK(false) << "unknow filter type";
  }
  return FilterPtr(nullptr);
}


FilterConfig* Filter::find(FilterConfig::Type type, Task* task) {
  for (int i = 0; i < task->filter_size(); ++i) {
    if (task->filter(i).type() == type) return task->mutable_filter(i);
  }
  return nullptr;
}

} // namespace PS
