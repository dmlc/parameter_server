/*
 * util.h
 *
 *  Created on: Apr 21, 2015
 *      Author: immars
 */

#ifndef SRC_APP_CAFFE_UTIL_H_
#define SRC_APP_CAFFE_UTIL_H_
#include <iostream>
#include <google/protobuf/io/coded_stream.h>
#include <glog/logging.h>
#include "util/common.h"

using namespace std;

void checkNAN(int count, const float* data, string blobName){
  bool isNan = false;
  int nanIndex = -1;
  int nanCount = 0;
  for (int j = 0; j < count; j++){
    if(isnan(data[j])){
      isNan = true;
      nanIndex = j;
      nanCount++;
    }
  }
  if(isNan){
    LL << nanCount << "NANs in "<< blobName <<"[" << nanIndex << "]!";
  }
}

inline unsigned long long tick(struct timeval* tv) {
  gettimeofday(tv, NULL);
  return tv->tv_sec * 1000000 + tv->tv_usec;
}

#endif /* SRC_APP_CAFFE_UTIL_H_ */
