#! /usr/bin/C
#option -cWall -p -cO2

#include "benchmark/benchmark.h"
#ifdef PICOGC_THREADED
#include "picogc/threaded.h"
#endif

#define LOOP_CNT 10000000

template<size_t SIZE> struct malloc_obj_t {
  std::string s_[SIZE]; // so that ~malloc_obj_t() will not be empty
};

template<size_t SIZE> struct gc_obj_t : public picogc::gc_object {
  std::string s_[SIZE]; // ditto
};

int main(int argc, char** argv)
{
  { // normal case
    benchmark_t bench("malloc");
    rng_t rng;

    for (int i = 0; i < LOOP_CNT; ++i) {
      switch ((rng() >> 8) & 15) {
#define CASE(n) case n: delete new malloc_obj_t<n>; break
	CASE(0);
	CASE(1);
	CASE(2);
	CASE(3);
	CASE(4);
	CASE(5);
	CASE(6);
	CASE(7);
	CASE(8);
	CASE(9);
	CASE(10);
	CASE(11);
	CASE(12);
	CASE(13);
	CASE(14);
	CASE(15);
#undef CASE
      }
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
	switch ((rng() >> 8) & 15) {
#define CASE(n) case n: new (picogc::IS_ATOMIC) gc_obj_t<n>; break
	  CASE(0);
	  CASE(1);
	  CASE(2);
	  CASE(3);
	  CASE(4);
	  CASE(5);
	  CASE(6);
	  CASE(7);
	  CASE(8);
	  CASE(9);
	  CASE(10);
	  CASE(11);
	  CASE(12);
	  CASE(13);
	  CASE(14);
	  CASE(15);
#undef CASE
	}
      }
    }

    gc.trigger_gc();
  }

  return 0;
}
