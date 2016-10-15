#include <stdio.h>

#ifdef __A
struct A {
  int i;
  A(int _i) { i = _i; puts("ctor A"); }
  ~A() { puts("dtor A"); }
};

A a(10);
#endif

#ifdef __B
struct B {
  int i;
  B(int _i) { i = _i; puts("ctor B"); }
  ~B() { puts("dtor B"); }
};

B b(20);
#endif

#if !defined(__A) && !defined(__B)
int main()
{
  puts("hello world!");
  return 0;
}
#endif
