#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <klee/klee.h>

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER; 
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
int data = 0;
pthread_key_t key;


void *consumer(void *d) {
  pthread_mutex_lock(&lock);
  fprintf(stderr, "consumer ");
  fprintf(stderr, "tid=%lx\n", pthread_self());
  int *p = (int *)d;
  while (*p == 0) {
    pthread_cond_wait(&cond, &lock);
  }
  --(*p);
  pthread_mutex_unlock(&lock);
  pthread_exit(NULL);
}

void *producer(void *d) {
  pthread_mutex_lock(&lock);
  fprintf(stderr, "producer ");
  fprintf(stderr, "tid=%lx\n", pthread_self());
  int *p = (int *)d;
  ++(*p);
  pthread_cond_signal(&cond);
  pthread_mutex_unlock(&lock);
  pthread_exit(NULL);
}

void *trier(void *d) {
  int rc = 0;
  if ((rc = pthread_mutex_trylock(&lock)) == 0) {
    pthread_mutex_unlock(&lock);
  }
  fprintf(stderr, "trier tid=%lx rc=%d\n", pthread_self(), rc);
  pthread_exit(NULL);
}

void *setter(void *d) {
  int *id = (int *) malloc(sizeof(int));
  *id = pthread_self();
  pthread_setspecific(key, id);
  fprintf(stderr, "setter load=%lx save=%lx\n", (uint64_t) pthread_getspecific(key),
         (uint64_t) id);
  free(id);
  pthread_exit(NULL);
}

int main(int argc, char **argv) {
  //klee_make_symbolic(&data, sizeof(int), "data");

  pthread_t p1;
  pthread_t p2;
  /*
   * pthread_t p3;
   * pthread_t p4;
   * pthread_t p5;
   */
  
  pthread_key_create(&key, NULL);

  pthread_create(&p1, NULL, consumer, (void *)&data);
  pthread_create(&p2, NULL, producer, (void *)&data);
  /*
   * pthread_create(&p3, NULL, trier, (void *)&data);
   * pthread_create(&p4, NULL, setter, (void *)&data);
   * pthread_create(&p5, NULL, setter, (void *)&data);
   */

  pthread_join(p1, NULL);
  pthread_join(p2, NULL);
  /*
   * pthread_join(p3, NULL);
   * pthread_join(p4, NULL);
   * pthread_join(p5, NULL);
   */

  pthread_key_delete(key);
}
