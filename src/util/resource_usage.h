#pragma once

#include "stdlib.h"
#include "stdio.h"
#include "string.h"

#include <ctime>
#include <ratio>
#include <chrono>

namespace PS {

using std::chrono::system_clock;
static system_clock::time_point tic() {
  return system_clock::now();
}

// return the time since tic, in second
static double toc(system_clock::time_point start) {
  size_t ct = std::chrono::duration_cast<std::chrono::milliseconds>(
      system_clock::now() - start).count();
  return (double) ct / 1e3;
}

// return the time since tic, in milliseconds
static double milliToc(system_clock::time_point start) {
  size_t ct = std::chrono::duration_cast<std::chrono::milliseconds>(
    system_clock::now() - start).count();
  return static_cast<double>(ct);
}

class ScopedTimer {
 public:
  explicit ScopedTimer(double* aggregate_time) :
      aggregate_time_(aggregate_time) {
    timer_ = tic();
  }
  ~ScopedTimer() { *aggregate_time_ += toc(timer_); }

 private:
  double* aggregate_time_;
  system_clock::time_point timer_;
};

// in senconds
class Timer {
 public:
  void start() { tp_ = tic(); }
  void restart() { reset(); start(); }
  void reset() { time_ = 0; }
  double stop() { time_ += toc(tp_); return time_; }
  double get() { return time_; }
  double getAndRestart() { double t = get(); reset(); start(); return t; }
 private:
  system_clock::time_point tp_ = tic();
  double time_ = 0;
};

// in milliseconds
class MilliTimer {
  public:
    void start() { tp_ = tic(); }
    void restart() { reset(); start(); }
    void reset() { time_ = 0; }
    double stop() { time_ += milliToc(tp_); return time_; }
    double get() { return time_; }
    double getAndRestart() { double t = get(); reset(); start(); return t; }

  private:
    system_clock::time_point tp_ = tic();
    double time_ = 0;
};

// http://stackoverflow.com/questions/63166/how-to-determine-cpu-and-memory-consumption-from-inside-a-process
// print memeory usage of the current process in Mb
// TODO CPU usage
class ResUsage {
 public:
  // in Mb
  static double myVirMem() {
    return getLine("/proc/self/status", "VmSize:") / 1e3;
  }
  static double myPhyMem() {
    return getLine("/proc/self/status", "VmRSS:") / 1e3;
  }
  // in Mb
  static double hostInUseMem() {
    return (getLine("/proc/meminfo", "MemTotal:") -
      getLine("/proc/meminfo", "MemFree:") -
      getLine("/proc/meminfo", "Buffers:") -
      getLine("/proc/meminfo", "Cached:")) / 1024;
  }
  static double hostTotalMem() {
    return getLine("/proc/meminfo", "MemTotal:") / 1024;
  }
 private:
  static double getLine(const char *filename, const char *name) {
    FILE* file = fopen(filename, "r");
    char line[128];
    int result = -1;
    while (fgets(line, 128, file) != NULL){
      if (strncmp(line, name, strlen(name)) == 0){
        result = parseLine(line);
        break;
      }
    }
    fclose(file);
    return result;
  }

  static int parseLine(char* line){
    int i = strlen(line);
    while (*line < '0' || *line > '9') line++;
    line[i-3] = '\0';
    i = atoi(line);
    return i;
  }
};

} // namespace PS
