#ifndef PLAYER_TIME_H_
#define PLAYER_TIME_H_

#include <assert.h>
#include <time.h>
#include "shrdef.h"


//
// Query
//

inline static int
timespec_get_minutes(const struct timespec span) {
  return span.tv_sec / 60;
}

inline static int
timespec_get_seconds_remaining(const struct timespec span) {
  return span.tv_sec % 60;
}

inline static int
timespec_get_miliseconds_remaining(const struct timespec span) {
  return span.tv_nsec / (1000 * 1000);
}

inline static long
timespec_get_miliseconds(const struct timespec span) {
  return span.tv_sec * 1000
    + timespec_get_miliseconds_remaining(span);
}

struct timespec
timespec_get_elapsed_between(
  const struct timespec start,
  const struct timespec end);

struct timespec
timer_get_elapsed(const struct timespec start);


//
// Command
//

static inline void
timer_start(struct timespec *start) {
  assert(start != NULL);
  assert(clock_gettime(CLOCK_MONOTONIC_RAW, start) == 0);
}

void
timer_add_elapsed(
    struct timespec *current_value,
    const struct timespec start);

#endif
