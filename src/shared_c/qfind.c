#include "qfind.h"

static inline ssize_t
qfind_should_insert_at(size_t pos) {
  return -(pos + 1);
}

ssize_t
qfind(
  const void *base,
  size_t nitems,
  size_t size,
  int (*compare)(const void* searched, const void* item),
  const void* searched) {
    assert(base != NULL);
    assert(size > 0);
    assert(compare != NULL);
    assert(searched != NULL);

    if (nitems == 0) {
      return qfind_should_insert_at(0);
    }

    size_t left = 0;
    size_t right = nitems - 1;
    for (;;) {
      size_t pos = (left + right) / 2;
      int result = compare(
        searched,
        (char*)base + pos * size);  // NOLINT
      if (result == 0) {
        return pos;
      } else if (result < 0) {
        if (pos == 0) {
          return qfind_should_insert_at(0);
        } else {
          right = pos - 1;
        }
      } else {
        if (pos == right) {
          return qfind_should_insert_at(right + 1);
        } else {
          left = pos + 1;
        }
      }
    }
  }
