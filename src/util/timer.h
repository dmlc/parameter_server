// moved to util/resource_usage.h

// #pragma once
// #include <sys/time.h>
// #include <iostream>

// namespace PS {

// #define MAX_ETIME 86400
// #define CLOCK_MONOTONIC_RAW 4

// // hardware clock, precision 1e-9
// inline timespec tic() {
//   timespec start;
//   clock_gettime(CLOCK_MONOTONIC_RAW, &start);
//   return start;
// }

// inline double toc(const timespec& start) {
//   timespec curr;
//   clock_gettime(CLOCK_MONOTONIC_RAW, &curr);
//   return (double) ((curr.tv_sec - start.tv_sec) +
//                    (curr.tv_nsec - start.tv_nsec)*1e-9);
// }

// elaspsed real time, precision 1e-6
// struct itimerval tic() {
//   struct itimerval start;
//   start.it_interval.tv_sec = 0;
//   start.it_interval.tv_usec = 0;
//   start.it_value.tv_sec = MAX_ETIME;
//   start.it_value.tv_usec = 0;
//   setitimer(ITIMER_REAL, &start, NULL);
//   return start;
// }

// double toc(const itimerval& start) {
//   itimerval curr;
//   getitimer(ITIMER_REAL, &curr);
//   // std::cerr <<  curr.it_value.tv_sec << " " << curr.it_value.tv_usec << std::endl;
//   return (double) ((start.it_value.tv_sec - curr.it_value.tv_sec) +
//                    (start.it_value.tv_usec - curr.it_value.tv_usec)*1e-6);
// }

// elaspsed user time, precision 1e-6
// inline itimerval tic_user() {
//   itimerval start;
//   start.it_interval.tv_sec = 0;
//   start.it_interval.tv_usec = 0;
//   start.it_value.tv_sec = MAX_ETIME;
//   start.it_value.tv_usec = 0;
//   setitimer(ITIMER_VIRTUAL, &start, NULL);
//   return start;
// }

// inline double toc_user(const itimerval& start) {
//   itimerval curr;
//   getitimer(ITIMER_VIRTUAL, &curr);
//   return (double) ((start.it_value.tv_sec - curr.it_value.tv_sec) +
//                    (start.it_value.tv_usec - curr.it_value.tv_usec)*1e-6);
// }



} // namespace PS
