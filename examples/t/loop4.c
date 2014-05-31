#include <stdio.h>
#include <stdlib.h>
int
main(int argc, char** argv) {
  int size;
  if (argc > 1) size = atoi(argv[1]);
  for (int i = 0; i < size; ++i) {
    [[loop_target]]for (int j = 0; j < size; ++j) {
      printf("%d:%d\n", i, j);
    }
  }
}
