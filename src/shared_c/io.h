#ifndef IO_H_
#define IO_H_

#include "shrdef.h"
#include "log.h"

/**
 * @brief memory block with defined size
 *
 */
struct io_memory_block {
  size_t size;
  void *data;
};

static inline bool
io_memory_block_is_empty(const struct io_memory_block *src) {
  assert(src != NULL);
  return src->data == NULL;
}

error_t
io_memory_block_alloc(
  size_t size,
  struct io_memory_block *result);

void
io_memory_block_free(struct io_memory_block *result);

/**
 * @brief IO stream statistics
 *
 */
struct io_stream_statistics {
  struct timespec waiting_time;
  struct timespec reading_time;
};

/**
 * @brief IO read-forward stream
 *
 */
struct io_rf_stream {
  char* name;
  int fd;
  struct io_memory_block buffer;
  size_t cursor_offset;
  size_t used_size;
};

static inline bool
io_rf_stream_is_empty(const struct io_rf_stream *src) {
  assert(src != NULL);
  assert(src->name != NULL);
  return src->used_size >= src->cursor_offset;
}

static inline bool
io_rf_stream_is_eof(const struct io_rf_stream *src) {
  assert(src != NULL);
  assert(src->name != NULL);
  return src->fd == -1;
}

static inline size_t
io_rf_stream_get_buffer_size(const struct io_rf_stream *src) {
  assert(src != NULL);
  assert(src->name != NULL);
  return src->buffer.size;
}

static inline size_t
io_rf_stream_get_unread_buffer_size(const struct io_rf_stream *src) {
  assert(src != NULL);
  assert(src->name != NULL);
  return src->used_size - src->cursor_offset;
}

struct io_memory_block
io_rf_stream_seek(struct io_rf_stream *src, size_t count);

/**
 * @brief Open file for reading
 *
 */
error_t
io_rf_stream_open_file(
  const char *file_path,
  size_t buffer_size,
  struct io_rf_stream *result);

/**
 * @brief Move unread part of the stream to
 * the begiggnig of the buffer and issue one "read" to fill it up.
 *
 * @param timeout maximum timeout for "poll"
 * @param stats optional for retrieving read statistics
 */
error_t
io_rf_stream_read(
  struct io_rf_stream *stream,
  int timeout,
  struct io_stream_statistics *stats);


error_t
io_rf_stream_seek_block(
  struct io_rf_stream *stream,
  int timeout,
  size_t result_size,
  void **result,
  struct io_stream_statistics *stats);

/**
 * @brief Move unread part of the stream to
 * the begiggnig of the buffer and fill it up.
 *
 * @param timeout maximum timeout for reading
 * @param stats optional for retrieving read statistics
 */
error_t
io_rf_stream_read_full(
  struct io_rf_stream *stream,
  int timeout,
  struct io_stream_statistics *stats);

void
io_rf_stream_free(struct io_rf_stream *stream);

static inline void
log_io_rf_stream_statistics(
    const struct io_stream_statistics *stats,
    const char *stream_name) {
  log_info(
    "Stream %s stats: waiting %d.%03ds, reading %d.%03ds",
    stream_name,
    stats->waiting_time.tv_sec,
    stats->waiting_time.tv_nsec / 1000000,
    stats->reading_time.tv_sec,
    stats->reading_time.tv_nsec / 1000000);
}

#endif