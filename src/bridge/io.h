#ifndef _H_IO
#define _H_IO

#include "./stddef.h"

struct io_memory_block {
  size_t size;
  void *data;
};

static inline bool
io_memory_block_is_empty(const struct io_memory_block *src) {
  return src->data == NULL;
}

error_t
io_alloc(
  size_t size,
  struct io_memory_block* result
);

error_t
io_read_file_memory(
  const char *filePath,
  struct io_memory_block* result
);

void
io_free_memory_block(struct io_memory_block* result);

#endif