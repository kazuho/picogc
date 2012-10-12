#! /usr/bin/C
#option -cWall -p -cg

#include <string>
#include "picogc.h"
#include "t/test.h"

using namespace std;

struct picogc::gc_stats gc_stats;

struct K : public picogc::gc_object {
  bool* dtor_called_;
  K(bool* dtor_called) : dtor_called_(dtor_called) {}
  ~K() {
    *dtor_called_ = true;
  }
};

void test()
{
  plan(2);
  
  picogc::gc gc;
  picogc::gc_scope gc_scope(&gc);
  bool inner_dtor_called = false;
  
  {
    picogc::scope scope;
    picogc::local<K> v = NULL;
    {
      v = new K(&inner_dtor_called);
    }
    gc.trigger_gc();
    ok(! inner_dtor_called);
  }
  gc.trigger_gc();
  ok(inner_dtor_called);
}
