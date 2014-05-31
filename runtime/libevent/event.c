#include <event.h>
#include <event2/event-config.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

// my own collection of event_base for use in event_init
#define MAX_EVENT_BASE 16
char event_bases[MAX_EVENT_BASE];
int event_base_next = 0;

// copied from event-internal.h
#define ev_callback ev_evcallback.evcb_cb_union.evcb_callback
#define ev_arg ev_evcallback.evcb_arg

// global queue for all events
typedef struct item {
  struct event *event;
  struct item *prev;
  struct item *next;
} item;
static struct item first = {NULL, NULL, NULL};
static struct item *head = &first;
static struct item *tail = &first;

static void enqueue(struct event *ev) {
  if (ev == NULL)
    return;
  tail->next = calloc(1, sizeof(struct item));
  tail->next->prev = tail;
  tail = tail->next;
  tail->event = ev;
  tail->next = NULL;
}

static void dequeue(struct event *ev) {
  struct item *prev, *cur;
  if (ev == NULL)
    return;
  for (prev = head; prev != tail; prev = prev->next) {
    cur = prev->next;
    if (cur->event->ev_fd == ev->ev_fd &&
        cur->event->ev_events == ev->ev_events &&
        cur->event->ev_callback == ev->ev_callback) {
      prev->next = cur->next;
      if (cur->next)
        cur->next->prev = prev;
      free(cur);
      break;
    }
  }
}

struct event_base *event_init(void) {
  if (event_base_next < MAX_EVENT_BASE)
    return (struct event_base *) &event_bases[event_base_next++];
  else
    return NULL;
}

void event_set(struct event *ev, evutil_socket_t fd, short events,
    void (*callback)(evutil_socket_t, short, void *), void *arg) {
  ev->ev_fd = fd;
  ev->ev_events = events;
  ev->ev_callback = callback;
  ev->ev_arg = arg;
}

int event_base_set(struct event_base *base, struct event *ev) {
  ev->ev_base = base;
  return 0;
}

int event_add(struct event *ev, const struct timeval *tv) {
  enqueue(ev);
  return 0;
}

int event_del(struct event *ev) {
  dequeue(ev);
  return 0;
}

/*
 * Sockets are implemented as symbolic files whose contents are always ready so
 * event_base_loop just goes through all events and process the ones that were
 * attached to it.
 */
int event_base_loop(struct event_base *base, int flags) {
  struct item *prev;
  for (prev = head; prev != tail; prev = prev->next) {
    struct event *ev = prev->next->event;
    /*
     * fprintf(stderr, "tid=%p, ev_base=%p, base=%p\n",
     *         (void *) pthread_self(), ev->ev_base, base); 
     */
    if (ev->ev_base == base) {
      (*ev->ev_callback)(ev->ev_fd, ev->ev_events, ev->ev_arg);
    }
  }
  return 0;
}

const char *event_get_version(void)
{
	return (EVENT__VERSION);
}
