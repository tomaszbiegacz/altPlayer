#ifndef PLAYER_TIME_H_
#define PLAYER_TIME_H_

#include <assert.h>
#include <time.h>
#include "shrdef.h"

unsigned
timespec_miliseconds(const struct timespec span);

struct timespec
timespec_elapsed_between(
  const struct timespec start,
  const struct timespec end);

inline static int
timespec_get_minutes(struct timespec span) {
  return span.tv_sec / 60;
}

inline static int
timespec_get_remaining_seconds(struct timespec span) {
  return span.tv_sec % 60;
}

static inline void
timer_start(struct timespec *start) {
  assert(start != NULL);
  assert(clock_gettime(CLOCK_MONOTONIC_RAW, start) == 0);
}

struct timespec
timer_elapsed(const struct timespec start);

void
timer_add_elapsed(
    struct timespec *current_value,
    const struct timespec start);

#endif