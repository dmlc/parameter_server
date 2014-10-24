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

} // namespace PS
