#ifndef test_h
#define test_h

#include <cstdio>

static void test();

static bool success_ = true;
static int n_ = 0;

static void plan(int num)
{
  printf("1..%d\n", num);
}

static void ok(bool b, const char* name = "")
{
  if (! b)
    success_ = false;
  printf("%s %d - %s\n", b ? "ok" : "not ok", ++n_, name);
}

template <typename T> void is(const T& x, const T& y, const char* name = "")
{
  if (x == y) {
    ok(true, name);
  } else {
    ok(false, name);
  }
}

int main(int, char**)
{
  test();
  return success_ ? 0 : 1;
}

#endif
