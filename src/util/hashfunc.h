#pragma once

#include <ctime>
#include <cstdlib>
#include "util/md5.h"
#include "util/key.h"

//TODO
// 1. add more hash function
// 2. give users flexibility to add more user-defined hash
namespace PS {

  class HashFunc {
    public:
      HashFunc() { srand(time(NULL)); }
      std::string GetHashForStr(std::string str);

      Key HashToKeyRange(Key min_key, Key max_key);
      Key AverageToKeyRange(const Key min_key, const Key max_key, const size_t num_com, const size_t i);
  };

}
