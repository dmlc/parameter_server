// some utility functions
#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <libgen.h>

// concurrency
#include <future>
#include <thread>
#include <mutex>
// smart pointers
#include <memory>
// stream
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <streambuf>
// containers
#include <vector>
#include <list>
#include <queue>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <algorithm>

#include <functional>


// google staff
#include "gflags/gflags.h"
#include "glog/logging.h"

// util
#include "util/macros.h"
#include "util/integral_types.h"
#include "util/resource_usage.h"

// base
#include <google/protobuf/stubs/common.h>
#include "google/protobuf/text_format.h"

//const int MAX_NUM_LEN = 1000;

namespace PS {

// uint64 is the default key size. We can change it into uint32 to reduce the
// spaces for storing the keys. Howerver, if we want a larger key size, say
// uint128, we need to change proto/range.proto to string type, because uint64
// is the largest integer type supported by protobuf
typedef uint64 Key;
static const Key kMaxKey = kuint64max;

typedef std::string NodeID;

typedef std::lock_guard<std::mutex> Lock;
using std::string;

#define LL LOG(ERROR)
#define LI LOG(INFO)

DECLARE_int32(num_threads);

// print the array's head and tail
template <typename V>
inline string dbstr(const V* data, int n, int m = 5) {
  std::stringstream ss;
  ss << "[" << n << "]: ";
  if (n < 2 * m) {
    for (int i = 0; i < n; ++i) ss << data[i] << " ";
  } else {
    for (int i = 0; i < m; ++i) ss << data[i] << " ";
    ss << "... ";
    for (int i = n-m; i < n; ++i) ss << data[i] << " ";
  }
  return ss.str();
}

#define NOTICE(_fmt_, args...) do {                                     \
    struct timeval tv; gettimeofday(&tv, NULL);                         \
    time_t ts = (time_t)(tv.tv_sec);                                    \
    struct ::tm tm_time; localtime_r(&ts, &tm_time);                    \
    int n = strlen(__FILE__) - 1;                                       \
    for (; n > -1; --n) { if (n==-1 || __FILE__[n] == '/') break; }     \
    fprintf(stdout, "[%02d%02d %02d:%02d:%02d.%03d %s:%d] " _fmt_ "\n", \
            1+tm_time.tm_mon, tm_time.tm_mday, tm_time.tm_hour,         \
            tm_time.tm_min, tm_time.tm_sec, (int)tv.tv_usec/1000,       \
            __FILE__+n+1, __LINE__, ##args);                            \
} while (0)

} // namespace PS


// basename(__FILE__)
