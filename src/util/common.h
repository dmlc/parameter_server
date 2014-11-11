// some utility functions
#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

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
#include <tuple>
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

typedef std::vector<Key> KeyList;

// profobuf. if we want to larger ones, such as uint128, we need to typedef uint64 Key;

typedef std::lock_guard<std::mutex> Lock;

using std::string;
typedef std::vector<std::string> StringList;

using std::shared_ptr;
using std::unique_ptr;
using std::pair;
using std::make_pair;
using std::map;
using std::tuple;
using std::make_tuple;
using std::to_string;
using std::initializer_list;

using google::protobuf::TextFormat;

#define LL LOG(ERROR)
#define LI LOG(INFO)
#define DD DLOG(ERROR)

DECLARE_int32(num_threads);

// http://stackoverflow.com/questions/109023/how-to-count-the-number-of-set-bits-in-a-32-bit-integer
static int32 NumberOfSetBits(int32 i) {
    i = i - ((i >> 1) & 0x55555555);
    i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
    return (((i + (i >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}

template <typename V>
static string dbstr(const V* data, int n, int m = 5) {
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



} // namespace PS
