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
  Linked* linked_;
  Linked() : super(true) {
    gc->trigger_gc();
    is(last_stats.collected, 0UL);
    if (num_created++ == 0)
      linked_ = new Linked();
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
    last_stats = stats;
  }
};

void test()
{
  plan(2);
  
  gc = new picogc::gc();
  gc->emitter(new Emitter());
  
  {
    picogc::scope scope(gc);
    new Linked();
  }
  
  gc->trigger_gc();
  is(last_stats.collected, 2UL);
}
