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
  int c = 0;
  if (argc > 1) c = atoi(argv[1]);

  [[loop_target]]for (int i = 0; i < c; ++i) {
    printf("%d:%d\n", i, subtotal(i));
  }

  return 0;
}
