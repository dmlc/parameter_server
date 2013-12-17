#pragma once

#include <ctime>
#include <cstdlib>
#include "util/key.h"

//TODO
// 1. add more hash function
// 2. give users flexibility to add more user-defined hash
namespace PS {

  class HashFunc {
    public:
      HashFunc() { srand(time(NULL)); }
      enum Type {
        EQUAL = 0,
        RAND = 1,
        MurMurHash3 = 2,
        MD5 = 3,
      };

      Key MMToKeyRange(Key min_key, Key max_key, const std::string& str);
      Key MD5ToKeyRange(Key min_key, Key max_key, const std::string& str);
      Key RandToKeyRange(Key min_key, Key max_key);
      Key AverageToKeyRange(const Key min_key, const Key max_key, const size_t num_com, const size_t i);
  };

}
