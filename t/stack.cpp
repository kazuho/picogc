#! /usr/bin/C
#option -cWall -p -cg

#include <string>
#include "picogc.h"
#include "t/test.h"

void test()
{
  plan(50003);
  
  picogc::_stack<int> stk;
  int* slot50000;

  for (int i = 0; i < 100000; ++i) {
    if (i == 50000) {
      slot50000 = stk.preserve();
    }
    *stk.push() = i;
  }
  is(*stk.pop(), 99999);
  stk.restore(slot50000);
  for (int i = 49999; i >= 0; --i) {
    int* slot = stk.pop();
    is(*slot, i);
  }
  ok(stk.pop() == NULL);
  ok(stk.pop() == NULL);
}
