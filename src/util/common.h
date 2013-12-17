// some utility functions
#pragma once
#include <stdio.h>
#include <stdlib.h>

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
// containers
#include <vector>
#include <map>
#include <tuple>
#include <set>
#include <algorithm>
// time
#include <ctime>
#include <ratio>
#include <chrono>

#include <unistd.h>


// google staff
// #include <glog/logging.h>

#include <gflags/gflags.h>

#include "util/macros.h"
#include "util/basictypes.h"
#include "util/join.h"
#include "util/logging.h"

// #include "base/callback.h"

#include <google/protobuf/stubs/common.h>

//const int MAX_NUM_LEN = 1000;

namespace PS {
typedef uint64 Key;
using std::string;
using std::shared_ptr;
using std::unique_ptr;
using std::pair;
using std::make_pair;
using std::map;
using std::tuple;
using std::make_tuple;
using operations_research::StrCat;
using google::protobuf::Closure;
using google::protobuf::NewCallback;
using google::protobuf::NewPermanentCallback;

// all about time
using std::chrono::system_clock;
using std::chrono::milliseconds;
using std::chrono::microseconds;
using std::chrono::seconds;

static system_clock::time_point tic() { return system_clock::now(); }
// return the time since tic in millionsecond
static int32 toc(system_clock::time_point start) {
  return std::chrono::duration_cast<milliseconds>(
      system_clock::now() - start).count();
}


// use string as the unique id to identify containers and inference
// algorithms. as we pass it quite office, between functions and machines, we
// may use a cheaper type late. TODO
typedef string name_t;


#define LL LOG(ERROR)

// split a string by delim. will not skip empty tokens, such as
// split("one:two::three", ':'); will return 4 items
static std::vector<std::string> split(const string &s, char delim) {
    std::vector<string> elems;
    std::stringstream ss(s);
    string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

// convert a non-string to string
template <class T>
static string strfy(const T& t) {
  std::ostringstream os;
  if(!(os << t)) {
    // TODO Handle the Error
    std::cerr << "[Error] strfy error!!!" << std::endl;
    exit(1);
  }
  return os.str();
}

//static string strfy(const string& str) {
//  return str;
//}
//
//static string strfy(const char* const& str) {
//  return str;
//}

static void strToUpper(string& s) {
  for (size_t i = 0; i < s.length(); i++) {
    s[i] = toupper(s[i]);
  }
}

// return <result, remainder> pair
static pair<string, PS::Key> div_mod(string dec_str, Key den) {
  Key rem = 0;
  string res;
  res.resize(1000);

  for(int indx=0, len = dec_str.length(); indx<len; ++indx) {
    rem = (rem * 10) + (dec_str[indx] - '0');
    res[indx] = rem / den + '0';
    rem %= den;
  }
  res.resize( dec_str.length() );

  while( res[0] == '0' && res.length() != 1)
    res.erase(0,1);

  if(res.length() == 0)
    res= "0";

  return make_pair(res, rem);
}

#define SINGLETON(Typename)                     \
  static Typename* Instance() {                 \
    static Typename e;                          \
    return &e;                                   \
  }                                             \

// http://stackoverflow.com/questions/109023/how-to-count-the-number-of-set-bits-in-a-32-bit-integer
static int32 NumberOfSetBits(int32 i) {
    i = i - ((i >> 1) & 0x55555555);
    i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
    return (((i + (i >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}

} // namespace PS
