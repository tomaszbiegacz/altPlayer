#ifndef _H_SHRDEF
#define _H_SHRDEF

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

static inline size_t
mimum_size_t(size_t a, size_t b) {
  return a < b ? a : b;
}

static inline size_t
maximum_size_t(size_t a, size_t b) {
  return a > b ? a : b;
}

#endif