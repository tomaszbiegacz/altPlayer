#pragma once
#include "io_buffer.h"

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


//
// Setup
//

/**
 * @brief Open file for reading
 *
 */
error_t
io_rf_stream_open_file(
  const char *file_path,
  size_t buffer_size,
  size_t buffer_max_single_read_size,
  struct io_rf_stream **handle);

void
io_rf_stream_release(struct io_rf_stream **handle);

//
// Query
//

static inline const struct io_buffer*
io_rf_stream_get_buffer(const struct io_rf_stream *src) {
  assert(src != NULL);
  return &src->buffer;
}

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


//
// Command
//

/**
 * @brief Check if there is anything to read before actually doing it
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
