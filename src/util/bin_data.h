#pragma once
#include "util/common.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace PS {

// manipulate binary data
template <class T>
static size_t load_bin(string const& filename, T **buf, long offset, size_t n) {
  FILE *fp = fopen(filename.c_str(), "rb");
  CHECK(fp != NULL) <<  "open " << filename << " for reading failed";
  if (*buf == NULL) *buf = new T[n];
  if (offset != 0)
    CHECK_EQ(fseek(fp, offset*sizeof(T), SEEK_SET),0) << "fseek failed";
  size_t ret = fread(*buf,sizeof(T),n,fp);
  fclose(fp);
  CHECK_EQ(ret, n) << "read " << filename << " failed";
  return ret;
}

template <class T>
static size_t bin_length(string const& filename) {
  struct stat file_stat;
  int ret = stat(filename.c_str(), &file_stat);
  if (ret == 0)
    return  (file_stat.st_size / sizeof(T));
  else
    return 0;
}

template <class T>
static size_t load_bin(string const& filename, T **buf) {
  return load_bin<T>(filename, buf, 0, bin_length<T>(filename));
}

template <class T>
static size_t save_bin(string const& filename, T *buff, size_t n) {
  FILE *fp = fopen(filename.c_str(), "wb");
  CHECK_NE(fp, NULL) << "open " << filename << " for writing failed";
  size_t ret = fwrite(buff,sizeof(T),n,fp);
  CHECK_EQ(ret, n) << "write " << filename << " failed";
  fclose(fp);
  return ret;
}

}
