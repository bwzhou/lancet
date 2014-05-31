#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

/* The index of the "read" end of the pipe */
#define READ 0

/* The index of the "write" end of the pipe */
#define WRITE 1

char *phrase = "Stuff this in your pipe and smoke it";
int fd[2];

void *writer(void *ignore) {
  /* Child Writer */
  /* close(fd[READ]); [> Close unused end<] */
  /*
   * if (read(fd[WRITE], message, 100) == -1) {
   *   [> error <]
   *   perror("Read from the writing side");
   * }
   * [> fill up the pipe buffer <]
   * for (int i = 0; i < 65499; ++i) {
   *   write(fd[WRITE], "a", 1);
   * }
   * [> would block until read <]
   * printf("Child:  Wrote %zd bytes to pipe!\n", write(fd[WRITE], phrase, strlen(phrase)+1));
   * sleep(5); // wait for a while before closing the pipe
   */
  write(fd[WRITE], phrase, strlen(phrase)+1);
  close(fd[WRITE]); /* Close used end*/
  printf("Child:  Wrote '%s' to pipe!\n", phrase);
  pthread_exit(NULL);
}

int main(int argc, char **argv) {
  int bytesRead;
  char message [100]; /* Parent process message buffer */
  pthread_t child;

  pipe(fd); /*Create an unnamed pipe*/

  pthread_create(&child, 0, writer, 0);
  
  /* Parent Reader */
  /* close(fd[WRITE]); [> Close unused end<] */
  /*
   * if (write(fd[READ], phrase, strlen(phrase)+1) == -1) {
   *   perror("Write into the reading side");
   * }
   * sleep(5);
   * for (int i = 0; i < 65499; ++i) {
   *   read(fd[READ], message, 1);
   * }
   */
  bytesRead = read(fd[READ], message, 100);
  /*
   * read(fd[READ], message, 100); // would block until the other end of the pipe is closed
   */
  close(fd[READ]); /* Close used end */
  printf("Parent: Read %d bytes from pipe: %s\n", bytesRead, message);

  pthread_join(child, NULL);
}
