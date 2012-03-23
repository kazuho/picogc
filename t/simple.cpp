#! /usr/bin/C
#option  -cWall -p -cg

#include <string>
#include "picogc.h"
#include "picogc.cpp"
#include "t/test.h"

using namespace std;

static vector<string> destroyed;

struct Label : public picogc::gc_object {
  typedef picogc::gc_object super;
  enum { HAS_GC_MEMBERS = 1 };
  string label_;
  picogc::member<Label> linked_;
  Label* init(const string& label) {
    label_ = label;
    return this;
  }
  virtual void gc_mark(picogc::gc* gc) {
    super::gc_mark(gc);
    gc->mark(linked_);
  }
  virtual void gc_destroy() {
    destroyed.push_back(label_);
    delete this;
  }
};

picogc::local<Label> doit()
{
  picogc::scope scope;
  
  picogc::local<Label> a = picogc::gc_new<Label>()->init("a");
  picogc::local<Label> b = picogc::gc_new<Label>()->init("b");
  picogc::local<Label> c = picogc::gc_new<Label>()->init("c");
  picogc::local<Label> d = picogc::gc_new<Label>()->init("d");
  a->linked_ = d;
  
  destroyed.clear();
  picogc::scope::top()->trigger_gc();
  ok(destroyed.empty(), "no objects destroyed");
  
  return scope.close(a);
}

void test()
{
  plan(6);
  
  picogc::gc gc;
  
  {
    picogc::scope scope(&gc);
    
    Label* ret = doit(); // scope.close() preserves the object
    
    destroyed.clear();
    gc.trigger_gc();
    is(destroyed.size(), 2UL, "two objects destroyed");
    
    ret->linked_ = NULL;
    
    destroyed.clear();
    gc.trigger_gc();
    is(destroyed.size(), 1UL, "one object destroyed");
    is(destroyed[0], string("d"), "object d destroyed");
    
    ret = NULL;
  }
  
  destroyed.clear();
  gc.trigger_gc();
  is(destroyed.size(), 1UL, "one object destroyed");
  is(destroyed[0], string("a"), "object a destroyed");
}
