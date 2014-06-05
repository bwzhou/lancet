#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fd.h"
#include "pipe.h"

static exe_file_t *__get_file(int fd) {
  if (fd >= 0 && fd < MAX_FDS) {
    exe_file_t *f = &__exe_env.fds[fd];
    if (f->flags & eOpen)
      return f;
  }
  return 0;
}

void pipe_init(struct pipe *p) {
  p->p_reader = 0;
  p->p_writer = 0;
  pthread_mutex_init(&p->p_mutex, 0);
  pthread_cond_init(&p->p_full, 0);
  pthread_cond_init(&p->p_empty, 0);
}

int pipe_read(struct pipe *p, char *buf, int bytes) {
  int r = 0;
  pthread_mutex_lock(&p->p_mutex);
  while (p->p_reader == p->p_writer) {
    pthread_cond_wait(&p->p_empty, &p->p_mutex);
  }
  while (r < bytes && p->p_reader != p->p_writer) {
    *(buf + r) = p->p_store[p->p_reader];
    ++r;
    p->p_reader = (p->p_reader + 1) % PIPE_SIZE;
  }
  pthread_cond_signal(&p->p_full);
  pthread_mutex_unlock(&p->p_mutex);
  return r;
}

static int __pipe_write_block(struct pipe *p, const char *buf, int bytes) {
  int r = 0;
  pthread_mutex_lock(&p->p_mutex);
  while ((p->p_writer + 1) % PIPE_SIZE == p->p_reader) {
    pthread_cond_wait(&p->p_full, &p->p_mutex);
  }
  while (r < bytes && (p->p_writer + 1) % PIPE_SIZE != p->p_reader) {
    p->p_store[p->p_writer] = *(buf + r);
    ++r;
    p->p_writer = (p->p_writer + 1) % PIPE_SIZE;
  }
  pthread_cond_signal(&p->p_empty);
  pthread_mutex_unlock(&p->p_mutex);
  return r;
}

int pipe_write(struct pipe *p, const char *buf, int bytes) {
  int r = 0;
  while (r < bytes) {
    r += __pipe_write_block(p, buf + r, bytes - r);
  }
  return r;
}

void pipe_close(struct pipe *p) {
  if (p && !__get_file(p->p_fdr) && !__get_file(p->p_fdw)) {
    free(p);
  }
}

int pipe(int pipefd[2]) {
  struct pipe *p;
  int fdr, fdw;
  exe_file_t *f;

  p = malloc(sizeof(struct pipe));
  pipe_init(p);

  for (fdr = 0; fdr < MAX_FDS; ++fdr) {
    if (!(__exe_env.fds[fdr].flags & eOpen))
      break;
  }
  if (fdr == MAX_FDS) {
    errno = EMFILE;
    return -1;
  }
  f = &__exe_env.fds[fdr];
  memset(f, 0, sizeof *f);
  f->flags = eOpen;
  f->pipe = p;
  LIST_INIT(&f->rq);
  LIST_INIT(&f->wq);
  
  p->p_fdr = fdr;

  for (fdw = 0; fdw < MAX_FDS; ++fdw) {
    if (!(__exe_env.fds[fdw].flags & eOpen))
      break;
  }
  if (fdw == MAX_FDS) {
    errno = EMFILE;
    return -1;
  }
  f = &__exe_env.fds[fdw];
  memset(f, 0, sizeof *f);
  f->flags = eOpen;
  f->pipe = p;
  LIST_INIT(&f->rq);
  LIST_INIT(&f->wq);
  
  p->p_fdw = fdw;

  pipefd[0] = fdr;
  pipefd[1] = fdw;

  return 0;
}
