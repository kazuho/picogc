#include <cstdio>
#include <string>
#include "picogc.h"

using namespace std;

struct Label : public picogc::gc_object {
  typedef picogc::gc_object super;
  string label_;
  picogc::member<Label> linked_;
  Label(const string& label) : label_(label), linked_(NULL) {}
  virtual void gc_mark(picogc::gc* gc) {
    super::gc_mark(gc);
    gc->mark(linked_);
  }
  virtual void gc_destroy() {
    printf("destroying label %s\n", label_.c_str());
    delete this;
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
  
  printf("triggering GC, but no objects should be freed\n");
  picogc::scope::top()->trigger_gc();
  
  return scope.close(a);
}

int main(int, char**)
{
  picogc::gc gc;
  
  {
    picogc::scope scope(&gc);
    
    Label* ret = doit(); // scope.close() preserves the object
    
    printf("triggering GC, b and c will be freed\n");
    gc.trigger_gc();
    
    ret->linked_ = NULL;
    
    printf("triggering GC, d will be freed\n");
    gc.trigger_gc();
    
    ret = NULL;
  }
  
  printf("triggering GC, a will be freed\n");
  gc.trigger_gc();
  
  return 0;
}
