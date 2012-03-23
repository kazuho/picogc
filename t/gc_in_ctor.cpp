#! /usr/bin/C
#option -cWall -p -cg

#include "picogc.h"
#include "picogc.cpp"
#include "t/test.h"

static picogc::gc* gc;
static picogc::gc_stats last_stats;
static size_t num_created = 0;

struct Linked : public picogc::gc_object {
  typedef picogc::gc_object super;
  enum { HAS_GC_MEMBERS = 1 };
  picogc::member<Linked> linked_;
  Linked() : super() {
    gc->trigger_gc();
    if (num_created++ == 0)
      linked_ = new Linked;
  }
  virtual void gc_mark(picogc::gc* gc) {
    super::gc_mark(gc);
    gc->mark(linked_);
  }
  virtual void gc_destroy() {
    delete this;
  }
};

struct Emitter : public picogc::gc_emitter {
  virtual void gc_end(picogc::gc*, const picogc::gc_stats& stats) {
    printf("%d\n", (int)stats.collected);
    last_stats = stats;
  }
};

void test()
{
  plan(0);
  
  gc = new picogc::gc();
  gc->emitter(new Emitter());

  picogc::scope scope(gc);
  picogc::gc_new<Linked>();
}
