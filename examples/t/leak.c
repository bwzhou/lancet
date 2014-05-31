#include <stdlib.h>
#include <string.h>
int main(int argc, char** argv) {
  int rv = 0;
  [[loop_target]]for (int i = 0; i < atoi(argv[4]); ++i) {
    rv = memcmp(argv[1], argv[2], atoi(argv[3]));
  }
  return rv;
}
