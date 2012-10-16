#! /usr/bin/C
#option -cWall -p -cO2

#include "benchmark/benchmark.h"
#ifdef PICOGC_THREADED
#include "picogc/threaded.h"
#endif

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
    benchmark_t bench("malloc");
    rng_t rng;

    for (int i = 0; i < LOOP_CNT; ++i) {
      delete new malloc_obj_t;
    }
  }

#ifdef PICOGC_THREADED
  picogc::threaded_gc gc;
#else
  picogc::gc gc;
#endif
  picogc::gc_scope gc_scope(&gc);
  { // GC case
    benchmark_t bench("picogc");
    picogc::scope scope;
    rng_t rng;

    for (int i = 0; i < LOOP_CNT / 100; ++i) {
      picogc::scope scope;
      for (int j = 0; j < 100; ++j) {
	new (picogc::IS_ATOMIC) gc_obj_t;
      }
    }

    gc.trigger_gc();
  }

  return 0;
}
