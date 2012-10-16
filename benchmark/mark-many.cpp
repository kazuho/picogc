#! /usr/bin/C
#option -cWall -p -cO2 -cDNDEBUG

#include "benchmark/benchmark.h"
#ifdef PICOGC_THREADED
#include "picogc/threaded.h"
#endif

#define MARK_CNT 100000
#define LOOP_CNT 10000000

struct malloc_link_t {
  malloc_link_t* next;
  malloc_link_t() : next(NULL) {}
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

int main(int argc, char** argv)
{
  { // normal case
    benchmark_t bench("malloc");

    malloc_link_t* head = new malloc_link_t;
    malloc_link_t* tail = head;
    for (int i = 0; i < MARK_CNT; ++i) {
      tail = tail->next = new malloc_link_t;
    }
    for (int i = 0; i < LOOP_CNT; ++i) {
      tail = tail->next = new malloc_link_t;
      malloc_link_t* t = head;
      head = head->next;
      delete t;
    }
  }

#ifdef PICOGC_THREADED
  picogc::threaded_gc gc;
#else
  picogc::gc gc; //(new picogc::config(102400));
#endif
  //gc.emitter(new picogc::gc_log_emitter(stdout));
  picogc::gc_scope gc_scope(&gc);
  { // GC case
    benchmark_t bench("picogc");
    picogc::scope scope;

    picogc::local<gc_link_t> head;
    {
      picogc::scope scope;
      head = new gc_link_t;
    }
    picogc::local<gc_link_t> tail = head;
    {
      picogc::scope scope;
      for (int i = 0; i < MARK_CNT; ++i) {
	tail->next = new gc_link_t;
	tail = tail->next;
      }
    }
    for (int i = 0; i < LOOP_CNT / 100; ++i) {
      picogc::scope scope;
      for (int j = 0; j < 100; ++j) {
	//printf("%p -> %p\n", &*head, head->next); fflush(stdout);
	tail->next = new gc_link_t;
	//printf("tail %p -> %p\n", &*tail, tail->next); fflush(stdout);
	tail = tail->next;
	head = head->next;
      }
    }
    gc.trigger_gc();
  }

  return 0;
}
