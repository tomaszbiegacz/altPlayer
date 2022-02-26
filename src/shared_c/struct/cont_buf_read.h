#pragma once
#include "../shrdef.h"

/**
 * @brief Continuous buffer with API for reading
 *
 */
struct cont_buf_read;

//
// Query
//

size_t
cont_buf_get_unread_size(const struct cont_buf_read *src);

static inline bool
cont_buf_read_is_empty(const struct cont_buf_read *src) {
  return cont_buf_get_unread_size(src) == 0;
}

//
// Commands
//

/**
 * @brief Try to read block of memory with given size.
 *
 * @param result pointer to block of memory
 */
bool
cont_buf_read_try(
  struct cont_buf_read *buf,
  size_t item_size,
  const void **result);

/**
 * @brief Begin reading buffer as an array.
 * Return number of items ready for reading.
 *
 */
size_t
cont_buf_read_array_begin(
  struct cont_buf_read *buf,
  size_t item_size,
  const void **result,
  size_t count);

/**
 * @brief Move reading cursor by given number of positions.
 *
 */
void
cont_buf_read_array_commit(
  struct cont_buf_read *buf,
  size_t item_size,
  size_t count);
