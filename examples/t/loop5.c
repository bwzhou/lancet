#include <stdio.h>
#include <stdlib.h>

int
subtotal(int N) {
  int i, sum = 0;
  for (i = 0; i < N; ++i) {
    sum += i;
  }
  return sum;
}

int
main(int argc, char** argv) {
  if (argc > 2) {
    int a = atoi(argv[1]);
    int b = atoi(argv[2]);
    [[loop_target]]for (int i = a; i < b; ++i) {
      printf("%d:%d\n", i, subtotal(i));
    }
  }
  return 0;
}
