#pragma once
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define UNUSED(x) (void)(x)
#define IF_NULL(x, default) x == NULL ? default:x
#define ARRAY_LENGTH(x) (sizeof(x) / sizeof(x[0]))
#define EMPTY_STRUCT(t, n) struct t n; memset(&n, 0, sizeof(n))

typedef void (*free_f) (void *arg);

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
