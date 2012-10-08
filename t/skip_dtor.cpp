#! /usr/bin/C
#option -cWall -p -cg

#include <string>
#include "picogc.h"
#include "t/test.h"

using namespace std;

struct GcStatsCollector : public picogc::gc_emitter {
  picogc::gc_stats stats_;
  virtual void gc_end(picogc::gc*, const picogc::gc_stats& stats) {
    stats_ = stats;
  }
};
static GcStatsCollector gcStatsCollector;

struct Link : public picogc::gc_object {
  static size_t dtor_call_count_;
  Link* next_;
  Link(Link* next) : picogc::gc_object(true, true), next_(next) {}
  virtual ~Link() {
    ++dtor_call_count_;
  }
  virtual void gc_mark(picogc::gc* gc) {
    picogc::gc_object::gc_mark(gc);
    gc->mark(next_);
  }
};

size_t Link::dtor_call_count_ = 0;

void test()
{
  plan(6);

  picogc::gc gc;
  gc.emitter(&gcStatsCollector);
  picogc::gc_scope gc_scope(&gc);

  {
    picogc::scope scope;
    picogc::local<Link> link = new Link(NULL);
    link->next_ = new Link(NULL);

    picogc::gc::top()->trigger_gc();

    is(gcStatsCollector.stats_.not_collected, (size_t)2);
    is(gcStatsCollector.stats_.collected, (size_t)0);
    is(Link::dtor_call_count_, (size_t)0);
  }

  picogc::gc::top()->trigger_gc();

  is(gcStatsCollector.stats_.not_collected, (size_t)0);
  is(gcStatsCollector.stats_.collected, (size_t)2);
    is(Link::dtor_call_count_, (size_t)0);
}
