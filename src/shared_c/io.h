#ifndef PLAYER_IO_H_
#define PLAYER_IO_H_

#include "shrdef.h"
#include "log.h"

const char*
get_filename_ext(const char *file_name);

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

error_t
io_buffer_alloc(
  size_t size,
  struct io_buffer *result);

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

void
io_buffer_free(struct io_buffer *src);

/**
 * @brief IO stream statistics
 *
 */
struct io_stream_statistics {
  struct timespec waiting_time;
  struct timespec reading_time;
};

error_t
io_buffer_write_from_read(
  struct io_buffer *dest,
  size_t max_read_size,
  int fd,
  bool *is_eof,
  struct io_stream_statistics *stats);

/**
 * @brief IO read-forward stream
 *
 */
struct io_rf_stream {
  char* name;
  int fd;
  struct io_buffer buffer;
  size_t buffer_max_single_read_size;
  struct io_stream_statistics *stats;
};

/**
 * @brief Open file for reading
 *
 */
error_t
io_rf_stream_open_file(
  const char *file_path,
  size_t buffer_size,
  size_t buffer_max_single_read_size,
  struct io_rf_stream *result);

static inline bool
io_rf_stream_is_eof(const struct io_rf_stream *src) {
  assert(src != NULL);
  assert(src->name != NULL);
  return src->fd == -1;
}

static inline bool
io_rf_stream_is_empty(const struct io_rf_stream *src) {
  return io_rf_stream_is_eof(src) && io_buffer_is_empty(&src->buffer);
}

static inline size_t
io_rf_stream_get_allocated_buffer_size(const struct io_rf_stream *src) {
  assert(src != NULL);
  return io_buffer_get_allocated_size(&src->buffer);
}

static inline size_t
io_rf_stream_get_unread_buffer_size(const struct io_rf_stream *src) {
  assert(src != NULL);
  return io_buffer_get_unread_size(&src->buffer);
}

static inline size_t
io_rf_stream_get_available_buffer_size(const struct io_rf_stream *src) {
  assert(src != NULL);
  return io_buffer_get_available_size(&src->buffer);
}

static inline bool
io_rf_stream_is_buffer_full(const struct io_rf_stream *src) {
  assert(src != NULL);
  return io_buffer_is_full(&src->buffer);
}

/**
 * @brief Check if there is anything to read before reading
 *
 */
error_t
io_rf_stream_read_with_poll(
  struct io_rf_stream *src,
  int poll_timeout);

/**
 * @brief Read block of memory from file or fail.
 *
 */
error_t
io_rf_stream_read(
  struct io_rf_stream *src,
  size_t item_size,
  void **dest);

static inline size_t
io_rf_stream_read_array(
  struct io_rf_stream *src,
  size_t item_size,
  void **dest,
  size_t max_count) {
    assert(src != NULL);
    return io_buffer_read_array(&src->buffer, item_size, dest, max_count);
  }

void
io_rf_stream_free(struct io_rf_stream *src);

#endif