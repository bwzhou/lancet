#include <stdio.h>
#include <stdlib.h>

int* create_matrix(
    int nrow,
    int ncol) {
  //printf("nrow=%d, ncol=%d\n", nrow, ncol);
  int* m = (int*) malloc(nrow * ncol * sizeof(int));
  int i, j;
  for (i = 0; i < nrow; ++i) {
    for (j = 0; j < ncol; ++j) {
      m[i * ncol + j] = rand();
    }
  }
  return m;
}

int* create_vector(
    int len) {
  //printf("len=%d\n", len);
  int* v = (int*) malloc(len * sizeof(int));
  int i;
  for (i = 0; i < len; ++i) {
    v[i] = rand();
  }
  return v;
}

// Computes mat * vec
int* multiply_matrix_vector(
    int nrow,
    int ncol, /* also the length of vec */
    int* mat,
    int* vec) {
  printf("nrow=%d, ncol=%d\n", nrow, ncol);
  int i, j;
  int* res = (int*) malloc(nrow * sizeof(int));
  for (i = 0; i < nrow; ++i) {
    res[i] = 0;
  }
  [[loop_target]]for (i = 0; i < nrow; ++i) {
    res[i] = 0;
    for (j = 0; j < ncol; ++j) {
      res[i] += mat[i * ncol + j] * vec[j];
    }
  }
  free(mat);
  free(vec);
  return res;
}

int main(
    int argc,
    char** argv) {
  int nrow = 0;
  int ncol = 0;
  int i;
  if (argc < 3) {
    printf("Usage: matvec nrow ncol.\n");
    exit(-1);
  }
  nrow = atoi(argv[1]);
  ncol = atoi(argv[2]);
  if (nrow <= 0 || ncol <= 0) {
    printf("Usage: matvec nrow ncol.\n");
    exit(-1);
  }
  srand(0);
  int* product = multiply_matrix_vector(nrow, ncol, create_matrix(nrow, ncol),
                                        create_vector(ncol));
  for (i = 0; i < nrow; ++i) {
    printf("%d ", product[i]);
  }
  printf("\n");
  free(product);

  return 0;
}
