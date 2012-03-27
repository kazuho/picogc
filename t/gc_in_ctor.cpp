#! /usr/bin/C
#option -cWall -p -cg

#include "picogc.h"
#include "t/test.h"

static picogc::gc* gc;
static picogc::gc_stats last_stats;
static size_t num_created = 0;

struct Linked : public picogc::gc_object {
  typedef picogc::gc_object super;
  Linked* linked_;
  Linked() : super(true) {
    gc->trigger_gc();
    is(last_stats.collected, (size_t) 0UL);
    if (num_created++ == 0)
      linked_ = new Linked();
  }
  virtual void gc_mark(picogc::gc* gc) {
    super::gc_mark(gc);
    gc->mark(linked_);
  }
};

struct Emitter : public picogc::gc_emitter {
  virtual void gc_end(picogc::gc*, const picogc::gc_stats& stats) {
    last_stats = stats;
  }
};

void test()
{
  plan(3);
  
  gc = new picogc::gc();
  gc->emitter(new Emitter());
  picogc::gc_scope gc_scope(gc);
  
  {
    picogc::scope scope;
    new Linked();
  }
  
  gc->trigger_gc();
  is(last_stats.collected, (size_t) 2UL);
}
