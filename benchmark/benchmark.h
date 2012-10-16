#ifndef benchmark_h
#define benchmark_h

extern "C" {
#include <sys/resource.h>
#include <sys/time.h>
}
#include "picogc.h"

class benchmark_t {
  std::string name_;
  double start_;
public:
  benchmark_t(const std::string& name) : name_(name), start_(now()) {}
  ~benchmark_t() {
    std::cout << name_ << "\t" << (now() - start_) << std::endl;
  }
  static double now() {
#if 0
    rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    return ru.ru_utime.tv_sec + ru.ru_utime.tv_usec / 1000000.0;
#else
    timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
#endif
  }
};

class rng_t {
  unsigned n_;
public:
  rng_t() : n_(1) {}
  int operator()() {
    n_ = n_ * 1103515245 + 12345;
    return n_ & 0x7fffffff;
  }
};

#endif
