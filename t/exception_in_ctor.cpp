#! /usr/bin/C
#option -cWall -p -cg

#include <string>
#include "picogc.h"
#include "t/test.h"

using namespace std;

struct picogc::gc_stats gc_stats;

struct gc_emitter : public picogc::gc_emitter {
  virtual void gc_end(picogc::gc*, const picogc::gc_stats& stats) {
    gc_stats = stats;
  }
};

struct K : public picogc::gc_object {
  static size_t dtor_called_;
  K() {
    throw 123;
  }
  ~K() {
    ++dtor_called_;
  }
};

size_t K::dtor_called_ = 0;

void test()
{
  plan(5);
  
  picogc::gc gc;
  gc.emitter(new gc_emitter);
  picogc::gc_scope gc_scope(&gc);
  
  {
    picogc::scope scope;
    
    try {
      new K();
    } catch (...) {
    }
    
    try {
      new (picogc::IS_ATOMIC) K();
    } catch (...) {
    }

    gc.trigger_gc();
    is(gc_stats.not_collected, (size_t)2);
    is(gc_stats.collected, (size_t)0);
  }
  
  gc.trigger_gc();
  is(gc_stats.not_collected, (size_t)0);
  is(gc_stats.collected, (size_t)2);
  is(K::dtor_called_, (size_t)0);
}
