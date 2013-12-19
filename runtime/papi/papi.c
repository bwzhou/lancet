#include <stdio.h>
#include <papi.h>
#include <unistd.h>
#include <sys/syscall.h>

pid_t gettid() { return syscall(SYS_gettid); }

#define NUM_EVENTS 3
static int events[NUM_EVENTS] = {
  //PAPI_L1_TCM,
  PAPI_L2_TCM,
  PAPI_TOT_INS,
  PAPI_TOT_CYC
};

static unsigned __papi_iterations = 0;

// Reentrant at loop header
void
__papi_wrapper_start()
{
  int e;
  e = PAPI_start_counters(events, NUM_EVENTS);
  if (e != PAPI_OK)
    fprintf(stderr, "PAPI: exit code %d tid %d func %s\n", e, gettid(), __func__);
  ++__papi_iterations;
}

// Reentrant at loop exits
void
__papi_wrapper_stop()
{
  int i, e;
  char name[PAPI_MAX_STR_LEN];
  long long values[NUM_EVENTS];

  e = PAPI_stop_counters(values, NUM_EVENTS);
  if (e != PAPI_OK)
    fprintf(stderr, "PAPI: exit code %d tid %d func %s\n", e, gettid(), __func__);

  for (i=0; i<NUM_EVENTS; ++i) {
    e = PAPI_event_code_to_name(events[i], name);
    if (e != PAPI_OK)
      fprintf(stderr, "PAPI: exit code %d\n", e);
    printf("PAPI: %s: %lld\n", name, values[i]);
  }
  printf("PAPI: iterations %u\n", __papi_iterations ? __papi_iterations - 1 : 0);

  printf("PAPI: number of hardware counters %d\n", PAPI_num_counters());
}
