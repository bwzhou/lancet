#include <assert.h>
#include <event.h>
#include <event2/event-config.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

// copied from event-internal.h
#define ev_callback ev_evcallback.evcb_cb_union.evcb_callback
#define ev_arg ev_evcallback.evcb_arg

#define MAX_EVENT_BASE 16
char __event_base_handle[MAX_EVENT_BASE];
int __event_base_next = 0;

struct item {
  struct event *event;
  volatile struct item *prev;
  volatile struct item *next;
};

struct queue {
  struct item first;
  volatile struct item *head;
  volatile struct item *tail;
};

struct queue __event_base_q[MAX_EVENT_BASE];
#define event_base_q(b) __event_base_q[(char *) b - __event_base_handle]

static void enqueue(struct event *ev) {
  if (ev == NULL || ev->ev_base == NULL)
    return;
  struct queue *bq = &event_base_q(ev->ev_base);
  bq->tail->next = calloc(1, sizeof(struct item));
  bq->tail->next->prev = bq->tail;
  bq->tail = bq->tail->next;
  bq->tail->event = ev;
  bq->tail->next = NULL;
}

static void dequeue(struct event *ev) {
  volatile struct item *prev, *cur;
  if (ev == NULL || ev->ev_base == NULL)
    return;
  struct queue *bq = &event_base_q(ev->ev_base);
  for (prev = bq->head; prev != bq->tail; prev = prev->next) {
    cur = prev->next;
    if (cur->event->ev_fd == ev->ev_fd &&
        cur->event->ev_events == ev->ev_events &&
        cur->event->ev_callback == ev->ev_callback) {
      prev->next = cur->next;
      if (cur->next)
        cur->next->prev = prev;
      free((void *) cur);
      break; // FIXME possible to have multiple matches?
    }
  }
}

struct event_base *event_init(void) {
  if (__event_base_next < MAX_EVENT_BASE) {
    struct queue *bq = &__event_base_q[__event_base_next];
    bq->first.event = NULL;
    bq->first.prev = NULL;
    bq->first.next = NULL;
    bq->head = &bq->first;
    bq->tail = &bq->first;
    return (struct event_base *) &__event_base_handle[__event_base_next++];
  }
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
/*
 * event_base_loop exits when no events are associated with the base
 */
int event_base_loop(struct event_base *base, int flags) {
  volatile struct item *prev;
  struct queue *bq = &event_base_q(base);
  for (prev = bq->head; prev != bq->tail; prev = prev->next) {
    struct event *ev = prev->next->event;
    /*
     * fprintf(stderr, "tid=%p, ev_base=%p, base=%p\n",
     *         (void *) pthread_self(), ev->ev_base, base); 
     */
    assert(ev->ev_base == base);
    (*ev->ev_callback)(ev->ev_fd, ev->ev_events, ev->ev_arg);
  }
  return 0;
}

const char *event_get_version(void)
{
	return (EVENT__VERSION);
}
