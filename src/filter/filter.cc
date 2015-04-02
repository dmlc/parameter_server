#include "filter/filter.h"
#include "filter/compressing.h"
#include "filter/key_caching.h"
#include "filter/fixing_float.h"
#include "filter/add_noise.h"

namespace PS {

Filter* Filter::create(const FilterConfig& conf) {
  switch (conf.type()) {
    case FilterConfig::KEY_CACHING:
      return new KeyCachingFilter();
    case FilterConfig::COMPRESSING:
      return new CompressingFilter();
    case FilterConfig::FIXING_FLOAT:
      return new FixingFloatFilter();
    case FilterConfig::NOISE:
      return new AddNoiseFilter();
    default:
      CHECK(false) << "unknow filter type";
  }
  return nullptr;
}


FilterConfig* Filter::find(FilterConfig::Type type, Task* task) {
  for (int i = 0; i < task->filter_size(); ++i) {
    if (task->filter(i).type() == type) return task->mutable_filter(i);
  }
  return nullptr;
}

} // namespace PS
