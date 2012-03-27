#! /usr/bin/C
#option -cWall -p -cg

#include <string>
#include "picogc.h"
#include "t/test.h"

using namespace std;

static vector<string> destroyed;

struct Label : public picogc::gc_object {
  typedef picogc::gc_object super;
  string label_;
  Label* linked_;
  Label(const string& label) : super(true), label_(label) {}
  virtual ~Label() {
    destroyed.push_back(label_);
  }
  virtual void gc_mark(picogc::gc* gc) {
    super::gc_mark(gc);
    gc->mark(linked_);
  }
};

picogc::local<Label> doit()
{
  picogc::scope scope;
  
  picogc::local<Label> a = new Label("a");
  picogc::local<Label> b = new Label("b");
  picogc::local<Label> c = new Label("c");
  picogc::local<Label> d = new Label("d");
  a->linked_ = d;
  
  destroyed.clear();
  picogc::gc::top()->trigger_gc();
  ok(destroyed.empty(), "no objects destroyed");
  
  return scope.close(a);
}

void test()
{
  plan(6);
  
  picogc::gc gc;
  picogc::gc_scope gc_scope(&gc);
  
  {
    picogc::scope scope;
    
    Label* ret = doit(); // scope.close() preserves the object
    
    destroyed.clear();
    gc.trigger_gc();
    is(destroyed.size(), (size_t) 2UL, "two objects destroyed");
    
    ret->linked_ = NULL;
    
    destroyed.clear();
    gc.trigger_gc();
    is(destroyed.size(), (size_t) 1UL, "one object destroyed");
    is(destroyed[0], string("d"), "object d destroyed");
    
    ret = NULL;
  }
  
  destroyed.clear();
  gc.trigger_gc();
  is(destroyed.size(), (size_t) 1UL, "one object destroyed");
  is(destroyed[0], string("a"), "object a destroyed");
}
