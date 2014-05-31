#include <stdio.h>
#include <stdlib.h>

void
print(int* a, int begin, int end) {
  for (int i = begin; i <= end; ++i)
    printf("%d ", a[i]);
  printf("\n");
}

void
merge(int* a, int begin, int middle, int end, int* b) {
  int i1 = begin;
  int i2 = middle;
  [[loop_target]]for (int j = begin; j <= end; ++j) {
    if (i1 < middle && (i2 > end || a[i1] < a[i2])) {
      b[j] = a[i1++];
    } else {
      b[j] = a[i2++];
    }
  }
  for (int j = begin; j <= end; ++j) {
    a[j] = b[j];
  }
}

void
mergesort(int* a, int begin, int end, int* b) {
  if (end - begin < 1) {
    return;
  }
  int middle = (begin + end) / 2;
  mergesort(a, begin, middle, b);
  mergesort(a, middle + 1, end, b);
  //print(a, begin, end);
  merge(a, begin, middle + 1, end, b);
  //print(a, begin, end);
}

int
main(int argc, char** argv) {
  int size = 0;
  if (argc > 1) size = atoi(argv[1]);
  if (size) {
    int* a = (int*) malloc(size * sizeof(int));
    int* b = (int*) malloc(size * sizeof(int));
    for (int i = 0; i < size; ++i) {
      a[i] = rand();
      b[i] = 0;
    }
    mergesort(a, 0, size - 1, b);
  }
}
