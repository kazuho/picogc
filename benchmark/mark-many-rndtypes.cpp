#! /usr/bin/C
#option -cWall -p -cO2 -cDNDEBUG

#include "benchmark/benchmark.h"

#define MARK_CNT 100000
#define LOOP_CNT 10000000

struct malloc_link_t {
  malloc_link_t* next;
  malloc_link_t() : next(NULL) {}
};

template <size_t SIZE> struct malloc_link_tmpl_t : public malloc_link_t {
  std::string s_[SIZE]; // so that dtor will not be empty
};

struct gc_link_t : public picogc::gc_object {
  gc_link_t* next;
  gc_link_t() : next(NULL) {
    //printf("alloc %p\n", this);
  }
  ~gc_link_t() {
    //printf("free %p\n", this);
    next = (gc_link_t*)0xdeadbaad;
  }
  void gc_mark(picogc::gc* gc) {
    gc->mark(next);
  }
};

template <size_t SIZE> struct gc_link_tmpl_t : public gc_link_t {
  std::string s_[SIZE];
};

inline malloc_link_t* create_malloc_link(rng_t& rng)
{
  switch ((rng() >> 8) & 15) {
#define CASE(n) case n: return new malloc_link_tmpl_t<n>
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

inline gc_link_t* create_gc_link(rng_t& rng)
{
  switch ((rng() >> 8) & 15) {
#define CASE(n) case n: return new gc_link_tmpl_t<n>
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

int main(int argc, char** argv)
{
  { // normal case
    benchmark_t bench("malloc");
    rng_t rng;

    malloc_link_t* head = create_malloc_link(rng);
    malloc_link_t* tail = head;
    for (int i = 0; i < MARK_CNT; ++i) {
      tail = tail->next = create_malloc_link(rng);
    }
    for (int i = 0; i < LOOP_CNT; ++i) {
      tail = tail->next = create_malloc_link(rng);
      malloc_link_t* t = head;
      head = head->next;
      delete t;
    }
  }

  picogc::gc gc; //(new picogc::config(102400));
  //gc.emitter(new picogc::gc_log_emitter(stdout));
  picogc::gc_scope gc_scope(&gc);
  { // GC case
    benchmark_t bench("picogc");
    rng_t rng;
    picogc::scope scope;

    picogc::local<gc_link_t> head;
    {
      picogc::scope scope;
      head = create_gc_link(rng);
    }
    picogc::local<gc_link_t> tail = head;
    {
      picogc::scope scope;
      for (int i = 0; i < MARK_CNT; ++i) {
	tail->next = create_gc_link(rng);
	tail = tail->next;
      }
    }
    for (int i = 0; i < LOOP_CNT / 100; ++i) {
      picogc::scope scope;
      for (int j = 0; j < 100; ++j) {
	//printf("%p -> %p\n", &*head, head->next); fflush(stdout);
	tail->next = create_gc_link(rng);
	//printf("tail %p -> %p\n", &*tail, tail->next); fflush(stdout);
	tail = tail->next;
	head = head->next;
      }
    }
    gc.trigger_gc();
  }

  return 0;
}
