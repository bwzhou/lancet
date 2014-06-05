#include <assert.h>
#include <event.h>
#include <event2/event-config.h>
#include <pthread.h>
#include <stdbool.h>
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
  struct item *prev;
  struct item *next;
  struct event *event;
  // Number of times the callback should be executed.
  // e.g. memcached::thread_libevent_process reads one
  // byte at each invocation but there may be multiple
  // bytes waiting in the pipe.
  int pending;
  void *rq_entry;
  void *wq_entry;
};

struct queue {
  struct item first;
  struct item *head;
  struct item *tail;
  pthread_cond_t cond;
  pthread_mutex_t mutex;
};

struct queue __event_base_q[MAX_EVENT_BASE];
#define event_base_q(b) __event_base_q[(char *) b - __event_base_handle]

static void lock(struct queue *q) {
  assert(q);
  pthread_mutex_lock(&q->mutex);
}

static void unlock(struct queue *q) {
  assert(q);
  pthread_mutex_unlock(&q->mutex);
}

static void wait(struct queue *q) {
  assert(q);
  pthread_cond_wait(&q->cond, &q->mutex);
}

static void signal(struct queue *q) {
  assert(q);
  pthread_cond_signal(&q->cond);
}

void *__fd_register_read_listener(int, void *, void (*)(void *));
void *__fd_register_write_listener(int, void *, void (*)(void *));
void __fd_cancel_listener(void *);

static void __event_activate(void *i) {
  struct item *it = (struct item *) i;
  struct queue *bq = &event_base_q(it->event->ev_base);
  lock(bq);
  ++it->pending;
  signal(bq);
  unlock(bq);
}

// A better way to find item from event?
void event_active(struct event *ev, int what, short ncalls) {
  struct item *prev, *cur;
  if (ev == NULL || ev->ev_base == NULL)
    return;
  struct queue *bq = &event_base_q(ev->ev_base);
  lock(bq);
  for (prev = bq->head; prev && prev->next; prev = prev->next) {
    cur = prev->next;
    if (cur->event->ev_fd == ev->ev_fd &&
        cur->event->ev_events == ev->ev_events &&
        cur->event->ev_callback == ev->ev_callback) {
      ++cur->pending;
      signal(bq); // FIXME What if multiple matches?
      break;
    }
  }
  unlock(bq);
}

static void enqueue(struct event *ev) {
  if (ev == NULL || ev->ev_base == NULL)
    return;
  struct queue *bq = &event_base_q(ev->ev_base);
  struct item *i = calloc(1, sizeof(struct item));
  i->prev = NULL;
  i->next = NULL;
  i->event = ev;
  i->pending = 0;
  i->rq_entry = ev->ev_events & EV_READ ?
    __fd_register_read_listener(ev->ev_fd, i, __event_activate) : NULL;
  i->wq_entry = ev->ev_events & EV_WRITE ?
    __fd_register_write_listener(ev->ev_fd, i, __event_activate) : NULL;

  lock(bq);
  bq->tail->next = i;
  i->prev = bq->tail;
  bq->tail = i;

  /*
   * TODO get rid of bq->tail by insert after bq->head
   *
   * i->next = bq->head->next;
   * bq->head->next->prev = i;
   * bq->head->next = i;
   * i->prev = bq->head;
   */

  printf("enqueue state %p item %p\n", (void *) pthread_self(),
         (void *) i);

  signal(bq); // FIXME Necessary?
  unlock(bq);
}

static void dequeue(struct event *ev) {
  struct item *prev, *cur;
  if (ev == NULL || ev->ev_base == NULL)
    return;
  struct queue *bq = &event_base_q(ev->ev_base);
  lock(bq);
  for (prev = bq->head; prev && prev->next; prev = prev->next) {
    cur = prev->next;
    if (cur->event->ev_fd == ev->ev_fd &&
        cur->event->ev_events == ev->ev_events &&
        cur->event->ev_callback == ev->ev_callback) {

      __fd_cancel_listener(cur->rq_entry);
      __fd_cancel_listener(cur->wq_entry);

      prev->next = cur->next;
      if (cur->next)
        cur->next->prev = prev;
      else // Fix: cur may be the last item
        bq->tail = prev;

      free((void *) cur);

      printf("dequeue state %p item %p\n", (void *) pthread_self(),
             (void *) cur);

      signal(bq); // FIXME What if multiple matches?
                  // FIXME Necessary?
      break;
    }
  }
  unlock(bq);
}

struct event_base *event_init(void) {
  if (__event_base_next < MAX_EVENT_BASE) {
    struct queue *bq = &__event_base_q[__event_base_next];
    bq->first.event = NULL;
    bq->first.prev = NULL;
    bq->first.next = NULL;
    bq->head = &bq->first;
    bq->tail = &bq->first;
    pthread_cond_init(&bq->cond, 0);
    pthread_mutex_init(&bq->mutex, 0);
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
  struct item *prev, *cur;
  struct queue *bq = &event_base_q(base);
  struct event *ev;
  lock(bq);
  while (bq->head->next) {
    /* 
     * Fix: ev_callback can free the current item so
     * prev->next may become nil inside the loop
     */
    for (prev = bq->head; prev && prev->next; prev = prev->next) {
      cur = prev->next;
      ev = cur->event;

      assert(ev->ev_base == base);
      /* 
       * Fix: ev_callback can free the current item so
       * cache the value of cur->pending in a local variable
       */
      int count = cur->pending;
      cur->pending = 0;
      while (count-- > 0) {

        /*
         * printf("state %p base %p event %p pending %d\n",
         *         (void *) pthread_self(), (void *) ev->ev_base, (void *) ev,
         *         count); 
         */

        unlock(bq); // Fix: prevent deadlock between bq and pipe
        (*ev->ev_callback)(ev->ev_fd, ev->ev_events, ev->ev_arg);
        /* 
         * Fix: ev_callback can free the current item so
         * the item should not be accessed after the callback
         */
        lock(bq);
      }
    }
    wait(bq);
  }
  unlock(bq);
  return 0;
}

const char *event_get_version(void)
{
  return (EVENT__VERSION);
}
