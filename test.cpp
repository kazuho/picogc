#include <cstdio>
#include <string>
#include "picogc.h"

using namespace std;

struct Label : public picogc::gc_object {
  string label_;
  picogc::member<Label> next_;
  Label(const string& label) : label_(label), next_(NULL) {}
  virtual void gc_mark(picogc::gc* gc) {
    gc->mark(next_);
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
  
  printf("triggering GC, but no objects should be freed\n");
  picogc::scope::top()->trigger_gc();
  
  return scope.close(a);
}

int main(int, char**)
{
  picogc::gc gc;
  picogc::scope scope(&gc);
  
  picogc::local<Label> ret = doit();
  
  printf("triggering GC, b and c will be freed\n");
  gc.trigger_gc();
  
  ret = NULL;
  
  printf("triggering GC, a will be freed\n");
  gc.trigger_gc();
  
  return 0;
}
