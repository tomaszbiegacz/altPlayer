#pragma once
#include "shrdef.h"

/**
 * @brief Quick find returning position of found element
 * or place where it should be inserted
 *
 * @param base pointer to the first element in the array
 * @param nitems number of items in the array
 * @param size size of each element
 * @param compare compare serched element with element from the array
 * @param searched pointer to searched element
 * @return ssize_t position where element has been found or where it should
 * be inserted
 */
ssize_t
qfind(
  const void *base,
  size_t nitems,
  size_t size,
  int (*compare)(const void* searched, const void* item),
  const void* searched);

static inline bool
qfind_is_found(ssize_t pos) {
  return pos >= 0;
}

static inline size_t
qfind_get_pos_to_insert(ssize_t pos) {
  assert(pos < 0);
  return -(pos + 1);
}
