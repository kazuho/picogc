#! /usr/bin/C
#option -cWall -p -cg

#include <string>
#include "picogc.h"
#include "t/test.h"

template <bool HasMembers> struct K : public picogc::gc_object {
  static size_t count_;
  virtual void gc_mark(picogc::gc* gc) {
    ++count_;
  }
};

template <bool HasMembers> size_t K<HasMembers>::count_;

void test()
{
  plan(2);

  picogc::gc gc;
  picogc::gc_scope gc_scope(&gc);

  {
    picogc::scope scope;

    new K<true>;
    new (picogc::IS_ATOMIC) K<false>;

    gc.trigger_gc();
    is(K<true>::count_, (size_t)1);
    is(K<false>::count_, (size_t)0);
  }
}
