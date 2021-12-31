#include "timer.h"

#define _NANOSECONDS_IN_SECOND (1000 * 1000 * 1000)

struct timespec
timespec_get_elapsed_between(
  const struct timespec start,
  const struct timespec end) {
    struct timespec result;
    if (end.tv_nsec < start.tv_nsec) {
        result.tv_sec = end.tv_sec-start.tv_sec-1;
        result.tv_nsec = _NANOSECONDS_IN_SECOND+end.tv_nsec-start.tv_nsec;
    } else {
        result.tv_sec = end.tv_sec-start.tv_sec;
        result.tv_nsec = end.tv_nsec-start.tv_nsec;
    }
    return result;
  }

struct timespec
timer_get_elapsed(const struct timespec start) {
  struct timespec end;
  assert(clock_gettime(CLOCK_MONOTONIC_RAW, &end) == 0);
  return timespec_get_elapsed_between(start, end);
}

void
timer_add_elapsed(
    struct timespec *current_value,
    const struct timespec start) {
  struct timespec elapsed = timer_get_elapsed(start);
  current_value->tv_nsec += elapsed.tv_nsec;
  current_value->tv_sec += elapsed.tv_sec;
  if (current_value->tv_nsec >= _NANOSECONDS_IN_SECOND) {
    current_value->tv_nsec -= _NANOSECONDS_IN_SECOND;
    current_value->tv_sec += 1;
  }
}
