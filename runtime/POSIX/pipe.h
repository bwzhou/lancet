#ifndef __PIPE_H__
#define __PIPE_H__

#include <pthread.h>
/*
 * See PIPE(7)
 *
 * A pipe has a limited capacity. If the pipe is full, then a write(2) will
 * block or fail, depending on whether the O_NONBLOCK flag is set. Different
 * implementations have different limits for the pipe capacity. In Linux
 * versions before 2.6.11, the capacity of a pipe was the same as the system
 * page size (e.g., 4096 bytes on i386). Since Linux 2.6.11, the pipe capacity
 * is 65536 bytes.
 */
#define PIPE_SIZE 65537
struct pipe {
  char p_store[PIPE_SIZE];
  int p_reader;
  int p_writer;
  pthread_mutex_t p_mutex;
  pthread_cond_t p_full;
  pthread_cond_t p_empty;
  int p_fdr;
  int p_fdw;
};

void pipe_init(struct pipe *p);
int pipe_read(struct pipe *p, char *buf, int bytes);
int pipe_write(struct pipe *p, const char *buf, int bytes);
void pipe_close(struct pipe *p);

#endif
