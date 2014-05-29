#include <event.h>
#include <pthread.h>
#include <sys/time.h>

struct event_base *event_init(void) {
  return NULL;
}

void event_set(struct event *ev, evutil_socket_t fd, short events,
    void (*callback)(evutil_socket_t, short, void *), void *arg) {
}

int event_base_set(struct event_base *base, struct event *ev) {
  return 0;
}

int event_add(struct event *ev, const struct timeval *tv) {
  return 0;
}

int event_del(struct event *ev) {
  return 0;
}

int event_base_loop(struct event_base *base, int flags) {
  return 0;
}
