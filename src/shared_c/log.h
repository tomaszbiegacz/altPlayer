#ifndef _H_LOG
#define _H_LOG

#include <time.h>
#include "shrdef.h"

extern bool _log_is_verbose;

/**
 * Initializes diagnostics.
 */
error_t
log_start();

/**
 * Free resources allocated by diagnostics module.
 * Should be called when program ends.
 */
void
log_free();

/**
 * is verbose diagnostics enabled?
 */
static inline bool
log_is_verbose() {
  return _log_is_verbose;
}

/**
 * enable or disable printing out verbose logs
 */
void
log_set_verbose(bool value);

/**
 * append logs to additional output file stream
 */
error_t
log_open_output_st(const char *path);

/**
 * always print to stdout
 */
void
log_info(const char *format, ...);

/**
 * log if verbose diagnostics is enabled
 */
void
log_verbose(const char *format, ...);

/**
 * always print to stderr
 */
void
log_error(const char *format, ...);

/**
 * if verbose diagnostics is enabled
 * write extended system information like OS name, avaiable memory etc
 */
void
log_full_system_information();

/**
 * if verbose diagnostics is enabled
 * write basic system information like avaiable memory etc
 */
void
log_system_information();

static inline void
log_start_timer(struct timespec* start) {
  assert(clock_gettime(CLOCK_MONOTONIC, start) == 0);
}

#define NANOSECONDS_IN_SECOND 1000000000

static inline struct timespec
log_stop_timer(const struct timespec start) {
  struct timespec end;
  assert(clock_gettime(CLOCK_MONOTONIC, &end) == 0);

  struct timespec result;
  if ((end.tv_nsec-start.tv_nsec)<0) {
      result.tv_sec = end.tv_sec-start.tv_sec-1;
      result.tv_nsec = NANOSECONDS_IN_SECOND+end.tv_nsec-start.tv_nsec;
  } else {
      result.tv_sec = end.tv_sec-start.tv_sec;
      result.tv_nsec = end.tv_nsec-start.tv_nsec;
  }
  return result;
}

static inline void
log_add_elapsed_time(
  struct timespec* current_value,
  const struct timespec start) {
  struct timespec elapsed = log_stop_timer(start);
  current_value->tv_nsec += elapsed.tv_nsec;
  current_value->tv_sec += elapsed.tv_sec;
  if (current_value->tv_nsec >= NANOSECONDS_IN_SECOND) {
    current_value->tv_nsec -= NANOSECONDS_IN_SECOND;
    current_value->tv_sec += 1;
  }
}

#endif
