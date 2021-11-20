#ifndef _H_IO
#define _H_IO

#include "shrdef.h"

//
// io_memory_block
//

struct io_memory_block {
  size_t size;
  void *data;
};

static inline bool
io_memory_block_is_empty(const struct io_memory_block *src) {
  return src->data == NULL;
}

void
io_memory_block_free(struct io_memory_block* result);

error_t
io_memory_block_alloc(
  size_t size,
  struct io_memory_block* result
);

error_t
io_memory_block_read_file(
  const char *file_path,
  struct io_memory_block* result
);

//
// io_fd_cursor
//

struct io_fd_cursor {
  char* name;
  int fd;
  struct io_memory_block buffer;
  size_t used_size;
};

struct io_fd_cursor_statistics {
  size_t loops_count;
  struct timespec waiting_time;
  struct timespec reading_time;
};

static inline bool
io_fd_cursor_is_eof(const struct io_fd_cursor *src) {
  assert(src->name != NULL);
  return src->fd == -1;
}

static inline struct io_memory_block
io_fd_cursor_to_memory_block(const struct io_fd_cursor *src) {
  assert(src->name != NULL);
  return (struct io_memory_block) {
    .size = src->used_size,
    .data = src->buffer.data,
  };
}

void
io_fd_cursor_free(struct io_fd_cursor* result);

error_t
io_fd_cursor_file(
  const char *file_path,
  size_t buffer_size,
  struct io_fd_cursor* result
);

error_t
io_fd_cursor_read(
  struct io_fd_cursor* result,
  int timeout
);

error_t
io_fd_cursor_read_full_stats(
  struct io_fd_cursor* result,
  struct io_fd_cursor_statistics* stats
);

inline static error_t
io_fd_cursor_read_full(
  struct io_fd_cursor* result
) {
  return io_fd_cursor_read_full_stats(result, NULL);
}

#endif