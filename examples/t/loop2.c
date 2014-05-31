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
getwidth(int n) {
  int i;
  
  for(i = 1; 1<<i <= n; ++i);
  printf("getwidth:%d,%d\n", n, i);
  return i;
}

int
main(int argc, char** argv) {
  int c = 0;
  if (argc > 1) c = atoi(argv[1]);

  if (c > 0) {
    int w = getwidth(c);
    [[loop_target]]for (int i = 0; i < w; ++i) {
      printf("%d:%d\n", i, subtotal(i));
    }
  }

  return 0;
}
