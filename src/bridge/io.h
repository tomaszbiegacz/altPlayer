#ifndef _H_IO
#define _H_IO

#include "./stddef.h"

struct io_memory_block {
  size_t size;
  void *start;
};

size_t
io_get_memory_in_pages(size_t value);

error_t
io_read_file_memory(
  const char *filePath,
  struct io_memory_block* result
);

void
io_free_memory_block(struct io_memory_block* result);

#endif