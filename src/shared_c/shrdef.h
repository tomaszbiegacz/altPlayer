#ifndef PLAYER_SHRDEF_H_
#define PLAYER_SHRDEF_H_

#include <stddef.h>
#include <stdbool.h>

/**
 * error_t may or may not be available from errno.h,
 * depending on the operating system.
 */
#ifndef __error_t_defined
#define __error_t_defined 1
  typedef int error_t;
#endif

static inline int
min_int(int a, int b) {
  return a < b ? a : b;
}

static inline int
max_int(int a, int b) {
  return a > b ? a : b;
}

static inline unsigned int
min_uint(unsigned int a, unsigned int b) {
  return a < b ? a : b;
}

static inline unsigned int
max_uint(unsigned int a, unsigned int b) {
  return a > b ? a : b;
}

static inline size_t
min_size_t(size_t a, size_t b) {
  return a < b ? a : b;
}

static inline size_t
max_size_t(size_t a, size_t b) {
  return a > b ? a : b;
}

#endif