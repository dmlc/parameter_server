#include "hashfunc.h"

namespace PS {

  std::string HashFunc :: GetHashForStr(std::string str) {
    MD5 md5(str);
    return md5.hexdigest();
  }

  Key HashFunc :: HashToKeyRange(Key min_key, Key max_key) {
    double r = rand() / (double) RAND_MAX;
    Key r_key = min_key + r * (max_key - min_key);
    
    return r_key;
  }

  Key HashFunc :: AverageToKeyRange(const Key min_key, const Key max_key, const size_t num, const size_t i) {
    size_t interval = (max_key - min_key) / num;

    Key r_key = min_key + i * interval;

    return r_key;
  }
}
