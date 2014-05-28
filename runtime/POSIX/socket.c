#define _LARGEFILE64_SOURCE
#include "fd.h"

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

static unsigned __next_socket_id = 0;

static exe_disk_file_t *__get_sym_socket() {
  if (__next_socket_id < __exe_fs.n_sym_socks)
    return &__exe_fs.sym_files[__exe_fs.n_sym_files + __next_socket_id++];
  else
    return NULL;
}

int socket(int domain, int type, int protocol) {
  exe_disk_file_t *df;
  exe_file_t *f;
  int fd;

  for (fd = 0; fd < MAX_FDS; ++fd) {
    if (!(__exe_env.fds[fd].flags & eOpen))
      break;
  }

  if (fd == MAX_FDS) {
    errno = EMFILE;
    return -1;
  }
  
  f = &__exe_env.fds[fd];

  /* Should be the case if file was available, but just in case. */
  memset(f, 0, sizeof *f);

  df = __get_sym_socket();
  if (df) {
    f->dfile = df;
    f->flags = eOpen | eReadable | eWriteable;
    return fd;
  } else {
    return -1;
  }
}

int bind(int sockfd, const struct sockaddr *my_addr, socklen_t addrlen) {
  return 0;
}

int listen(int sockfd, int backlog) {
  return 0;
}

/*
 * accept() is usually called in a loop so all sockets would be exhausted,
 * the number of symbolic sockets determines the number of connections.
 */
int accept(int sockfd, struct sockaddr *cliaddr, socklen_t *addrlen) {
  return socket(0, 0, 0);
}

int connect(int sockfd, const struct sockaddr *serv_addr, socklen_t addrlen) {
  return 0;
}
