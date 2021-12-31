#include <errno.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "io_buffer.h"
#include "log.h"
#include "timer.h"

#define LAST_IO_ERROR errno != 0 ? errno : EIO

error_t
io_buffer_alloc(
  size_t size,
  struct io_buffer *result) {
    assert(result != NULL);
    if (result->data != NULL && result->size_allocated < size) {
      io_buffer_free(result);
    }

    if (result->data == NULL) {
      void* mem_range = mmap(
        NULL,
        size,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        0, 0);

      if (mem_range == MAP_FAILED) {
        log_error(
          "Failed to allocate memory of size %d",
          size);
        return errno;
      } else {
        log_verbose(
          "Allocated %dkB of memory mapping starting at %x",
          size / 1024,
          (unsigned long)mem_range);
      }

      result->size_allocated = size;
      result->size_used = 0;
      result->start_offset = 0;
      result->data = mem_range;
    }

    return 0;
  }

inline static void*
io_buffer_data_start_read(struct io_buffer *src) {
  return (char*)src->data + src->start_offset; // NOLINT
}

inline static void*
io_buffer_data_start_write(struct io_buffer *src) {
  return (char*)src->data + src->size_used; // NOLINT
}

static void
io_buffer_no_padding(struct io_buffer *src) {
  if (src->start_offset > 0) {
    size_t unread_size = io_buffer_get_unread_size(src);
    memmove(
      src->data,
      io_buffer_data_start_read(src),
      unread_size);
    src->start_offset = 0;
    src->size_used = unread_size;
  }
}

bool
io_buffer_try_write(
  struct io_buffer *dest,
  size_t item_size,
  const void *src) {
    assert(item_size > 0);
    assert(src != NULL);
    size_t remaining = dest->size_allocated - io_buffer_get_unread_size(dest);
    if (remaining < item_size) {
      return false;
    } else {
      if (dest->size_allocated - dest->size_used < item_size) {
        io_buffer_no_padding(dest);
      }
      memcpy(
        io_buffer_data_start_write(dest),
        src,
        item_size);
      dest->size_used += item_size;
      return true;
    }
  }

bool
io_buffer_try_read(
  struct io_buffer *src,
  size_t item_size,
  void **result) {
    assert(item_size > 0);
    size_t available = io_buffer_get_unread_size(src);
    if (available >= item_size) {
      *result = io_buffer_data_start_read(src),
      src->start_offset += item_size;
      return true;
    } else {
      return false;
    }
  }

size_t
io_buffer_read_array(
  struct io_buffer *src,
  size_t item_size,
  void **result,
  size_t max_count) {
    assert(item_size > 0);
    assert(max_count > 0);
    size_t available = io_buffer_get_unread_size(src);
    size_t items_count = min_size_t(max_count, available / item_size);
    if (items_count > 0) {
      *result = io_buffer_data_start_read(src),
      src->start_offset += items_count * item_size;
    }
    return items_count;
  }

void
io_buffer_array_seek(
  struct io_buffer *src,
  size_t item_size,
  size_t items_count) {
    assert(src != NULL);
    size_t seek_size = items_count * item_size;
    assert(seek_size <= io_buffer_get_unread_size(src));
    src->start_offset += seek_size;
  }

void
io_buffer_array_items(
  struct io_buffer *src,
  size_t item_size,
  void **result,
  size_t *count) {
    assert(src != NULL);
    assert(result  != NULL);
    assert(count != NULL);
    *result = io_buffer_data_start_read(src);
    *count = io_buffer_get_unread_size(src) / item_size;
  }

void
io_buffer_free(struct io_buffer* result) {
  assert(result != NULL);
  if (result->data != NULL) {
    if (munmap(result->data, result->size_allocated) != 0) {
      log_error(
        "Cannot release memory mapping starting at %x due to",
        (unsigned long)result->data,
        strerror(errno));
    } else {
      log_verbose(
        "Released memory mapping starting at %x",
        (unsigned long)result->data);
      result->size_allocated = 0;
      result->size_used = 0;
      result->start_offset = 0;
      result->data = NULL;
    }
  }
}

error_t
io_buffer_write_from_read(
  struct io_buffer *dest,
  size_t max_read_size,
  int fd,
  bool *is_eof,
  struct io_stream_statistics *stats) {
    assert(dest->data != NULL);
    assert(max_read_size > 0);
    assert(fd != -1);
    assert(is_eof != NULL);

    if (dest->size_allocated == dest->size_used) {
      io_buffer_no_padding(dest);
    }
    assert(dest->size_allocated > dest->size_used);

    struct timespec wait_start;
    if (stats != NULL) {
      timer_start(&wait_start);
    }
    size_t read_size = min_size_t(
      max_read_size,
      dest->size_allocated - dest->size_used);
    ssize_t read_count = read(
      fd,
      io_buffer_data_start_write(dest),
      read_size);
    if (stats != NULL) {
      timer_add_elapsed(&stats->reading_time, wait_start);
    }

    if (read_count < 0) {
      if (errno == EINTR)
        // let's try again
        return io_buffer_write_from_read(
          dest, max_read_size, fd, is_eof, stats);
      else
        return LAST_IO_ERROR;
    } else {
      dest->size_used += read_count;
      *is_eof = read_count == 0;
      return 0;
    }
  }
