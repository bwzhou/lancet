#include <klee/klee.h>
int main(int argc, char **argv) {
  int N = 0;
  klee_make_symbolic(&N, sizeof(int), "N");
  int i = 0;
  int sum = 0;
  for (i = 0; i < N; ++i) {
    sum += i;
  }
  return sum;
}
