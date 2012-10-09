#! /usr/bin/C
#option -cWall -p -cO2

extern "C" {
#include <sys/time.h>
}
#include "picogc.h"

class benchmark_t {
  double start_;
public:
  benchmark_t() : start_(now()) {}
  double elapsed() {
    return now() - start_;
  }
  static double now() {
    timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
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

#define LOOP_CNT 10000000

struct malloc_obj_t {
  int i_;
  malloc_obj_t() : i_(0) {}
};

struct gc_obj_t : public picogc::gc_object {
  int i_;
  gc_obj_t() : i_(0) {}
};

int main(int argc, char** argv)
{
  { // normal case
    benchmark_t bench;
    rng_t rng;

    for (int i = 0; i < LOOP_CNT; ++i) {
      delete new malloc_obj_t;
    }

    std::cout << "malloc\t" << bench.elapsed() << endl;
  }

  picogc::gc gc;
  picogc::gc_scope gc_scope(&gc);
  { // GC case
    picogc::scope scope;
    benchmark_t bench;
    rng_t rng;

    for (int i = 0; i < LOOP_CNT / 100; ++i) {
      picogc::scope scope;
      for (int j = 0; j < 100; ++j) {
	new (picogc::IS_ATOMIC) gc_obj_t;
      }
    }

    gc.trigger_gc();

    std::cout << "picogc\t" << bench.elapsed() << endl;
  }

  return 0;
}
