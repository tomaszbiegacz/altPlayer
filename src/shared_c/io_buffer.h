#pragma once
#include <time.h>
#include "shrdef.h"

/**
 * @brief memory buffer
 *
 */
struct io_buffer {
  size_t size_allocated;
  size_t size_used;
  size_t start_offset;
  void *data;
};

/**
 * @brief IO stream statistics
 *
 */
struct io_stream_statistics {
  struct timespec waiting_time;
  struct timespec reading_time;
};


//
// Setup
//

error_t
io_buffer_alloc(
  size_t size,
  struct io_buffer *result);

void
io_buffer_free(struct io_buffer *src);


//
// Query
//

static inline size_t
io_buffer_get_allocated_size(const struct io_buffer *src) {
  assert(src != NULL);
  return src->size_allocated;
}

static inline size_t
io_buffer_get_unread_size(const struct io_buffer *src) {
  assert(src != NULL);
  return src->size_used - src->start_offset;
}

static inline bool
io_buffer_is_empty(const struct io_buffer *src) {
  return io_buffer_get_unread_size(src) == 0;
}

static inline size_t
io_buffer_get_available_size(const struct io_buffer *src) {
  return io_buffer_get_allocated_size(src) - io_buffer_get_unread_size(src);
}

static inline bool
io_buffer_is_full(const struct io_buffer *src) {
  return io_buffer_get_available_size(src) == 0;
}

//
// Command
//

bool
io_buffer_try_write(
  struct io_buffer *dest,
  size_t item_size,
  const void *src);

bool
io_buffer_try_read(
  struct io_buffer *src,
  size_t item_size,
  void **result);

size_t
io_buffer_read_array(
  struct io_buffer *src,
  size_t item_size,
  void **result,
  size_t max_count);

void
io_buffer_array_items(
  struct io_buffer *src,
  size_t item_size,
  void **result,
  size_t *count);

void
io_buffer_array_seek(
  struct io_buffer *src,
  size_t item_size,
  size_t items_count);

error_t
io_buffer_write_from_read(
  struct io_buffer *dest,
  size_t max_read_size,
  int fd,
  bool *is_eof,
  struct io_stream_statistics *stats);

