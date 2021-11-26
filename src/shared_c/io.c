#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "io.h"
#include "log.h"

#define LAST_IO_ERROR errno != 0 ? errno : EIO

error_t
io_memory_block_alloc(
    size_t size,
    struct io_memory_block* result) {
  assert(io_memory_block_is_empty(result));

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
      "Allocated memory mapping starting at %x",
      (unsigned long)mem_range);
  }

  log_system_information();
  result->data = mem_range;
  result->size = size;
  return 0;
}

void
io_memory_block_free(struct io_memory_block* result) {
  if (!io_memory_block_is_empty(result)) {
    if (munmap(result->data, result->size) != 0) {
      log_error(
        "Cannot release memory mapping starting at %x due to",
        (unsigned long)result->data,
        strerror(errno));
    } else {
      log_verbose(
        "Released memory mapping starting at %x",
        (unsigned long)result->data);
      result->data = NULL;
      result->size = 0;
    }
  }
}

static void
io_rf_stream_close_fd(struct io_rf_stream *result) {
  if (close(result->fd) == -1) {
    log_error(
      "Error when closing rf_stream [%s]",
      result->name,
      strerror(errno));
  }
  result->fd = -1;
}

void
io_rf_stream_free(struct io_rf_stream *result) {
  assert(result != NULL);

  if (result->name != NULL) {
    log_verbose("Closing rf_stream [%s]", result->name);
    if (result->fd != -1) {
      io_rf_stream_close_fd(result);
    }

    io_memory_block_free(&result->buffer);
    result->cursor_offset = 0;
    result->used_size = 0;

    free(result->name);
    result->name = NULL;
  }
}

struct io_memory_block
io_rf_stream_seek(struct io_rf_stream *src, size_t count) {
  assert(count <= io_rf_stream_get_unread_buffer_size(src));
  struct io_memory_block result = (struct io_memory_block) {
    .size = count,
    .data = (char*)src->buffer.data + src->cursor_offset, // NOLINT
  };
  src->cursor_offset += count;
  return result;
}

error_t
io_rf_stream_open_file(
    const char *file_path,
    size_t buffer_size,
    struct io_rf_stream *result) {
  assert(result != NULL);
  assert(result->name == NULL);

  error_t result_error = 0;
  result->fd = open(file_path, O_RDONLY | O_NONBLOCK);
  if (result->fd == -1) {
    log_error("Cannot open file [%s]", file_path);
    result_error = errno;
  }

  if (result_error == 0) {
    result_error = io_memory_block_alloc(
      buffer_size,
      &result->buffer);
  }

  if (result_error == 0) {
    result->name = strdup(file_path);
    if (result->name == NULL) {
      result_error = ENOMEM;
    }
  }

  result->cursor_offset = 0;
  result->used_size = 0;
  if (result_error != 0) {
    assert(result->name == NULL);

    if (result->fd != -1) {
      close(result->fd);
      result->fd = -1;
    }

    io_memory_block_free(&result->buffer);
  }
  return result_error;
}

static error_t
io_rf_stream_read_once(
    const char *name,
    int fd,
    size_t buffer_size,
    void *buffer,
    size_t *read_size,
    struct io_stream_statistics *stats) {
  assert(buffer_size > 0);

  struct timespec wait_start;
  if (stats != NULL) {
    log_timer_start(&wait_start);
  }
  ssize_t read_count = read(fd, buffer, buffer_size);
  if (stats != NULL) {
    log_add_elapsed_time(&stats->reading_time, wait_start);
  }

  if (read_count < 0) {
    switch (errno) {
      case EINTR:
        // let's try again
        return io_rf_stream_read_once(
          name, fd,
          buffer_size, buffer,
          read_size, stats);
      default:
        log_error(
          "Got an error when reading content of rf_stream [%s]",
          name);
        return LAST_IO_ERROR;
    }
  }

  *read_size = read_count;
  assert(*read_size <= buffer_size);
  return 0;
}

static error_t
io_rf_stream_read_with_waiting(
    const char *name,
    int fd,
    size_t buffer_size,
    void *buffer,
    int timeout,
    size_t *read_size,
    bool *is_eof,
    struct io_stream_statistics *stats) {
  struct pollfd pfd = (struct pollfd) {
    .fd = fd,
    .events = POLLIN
  };

  struct timespec wait_start;
  if (stats != NULL) {
    log_timer_start(&wait_start);
  }
  int ready = poll(&pfd, 1, timeout);
  if (stats != NULL) {
    log_add_elapsed_time(&stats->waiting_time, wait_start);
  }

  if (ready == -1) {
    log_error(
      "Poll on rf_stream [%s] failed with revents %d",
      name,
      pfd.revents);
    return LAST_IO_ERROR;
  }

  *is_eof = false;
  *read_size = 0;
  error_t error_r = 0;
  if (ready > 0 && pfd.revents != 0) {
    if (pfd.revents & POLLIN) {
      error_r = io_rf_stream_read_once(
        name, fd,
        buffer_size, buffer,
        read_size, stats);
      *is_eof = error_r == 0 && *read_size == 0;
    } else if (pfd.revents & POLLHUP) {
      *is_eof = true;
    } else {
      log_error(
        "Poll on rf_stream [%s] finished with revents %d",
        name,
        pfd.revents);
      error_r = EIO;
    }
  }
  return error_r;
}

static void
io_rf_stream_buffer_zero_padding(struct io_rf_stream* result) {
  if (result->cursor_offset > 0) {
    size_t unread_size = io_rf_stream_get_unread_buffer_size(result);
    memmove(
      result->buffer.data,
      (char*)result->buffer.data + result->cursor_offset, // NOLINT
      unread_size);
    result->cursor_offset = 0;
    result->used_size = unread_size;
  }
}

error_t
io_rf_stream_read(
    struct io_rf_stream* result,
    int timeout,
    struct io_stream_statistics *stats) {
  error_t error_r = 0;
  if (!io_rf_stream_is_eof(result)) {
    io_rf_stream_buffer_zero_padding(result);

    bool is_eof;
    size_t read_size;
    size_t buffer_size = result->buffer.size - result->used_size;
    void* buffer = (char*)result->buffer.data + result->used_size;  // NOLINT
    error_r  = io_rf_stream_read_with_waiting(
      result->name, result->fd,
      buffer_size, buffer,
      timeout,
      &read_size, &is_eof, stats);

    if (error_r == 0) {
      result->used_size += read_size;
      if (is_eof) {
        log_verbose(
          "Read end of rf_stream [%s], closing.",
          result->name);
        io_rf_stream_close_fd(result);
      }
    }
  }
  return error_r;
}

error_t
io_rf_stream_read_full(
    struct io_rf_stream* result,
    int timeout,
    struct io_stream_statistics* stats) {

  error_t error_r = 0;
  if (!io_rf_stream_is_eof(result)) {
    struct timespec start;
    log_timer_start(&start);
    int timer_used = 0;

    io_rf_stream_buffer_zero_padding(result);
    while (
        error_r == 0
        && result->used_size < result->buffer.size
        && result->fd != -1
        && timer_used < timeout) {
      error_r  = io_rf_stream_read(
        result,
        timeout - timer_used,
        stats);

      timer_used = log_timer_miliseconds(start);
    }
  }
  return error_r;
}

error_t
io_rf_stream_seek_block(
    struct io_rf_stream *stream,
    int timeout,
    size_t result_size,
    void** result,
    struct io_stream_statistics *stats) {
  assert(result_size > 0);
  assert(result != NULL);
  assert(result_size <= io_rf_stream_get_buffer_size(stream));
  error_t error_r = 0;
  if (io_rf_stream_get_unread_buffer_size(stream) < result_size) {
    error_r = io_rf_stream_read_full(stream, timeout, stats);
  }
  if (error_r == 0) {
    size_t available_size = io_rf_stream_get_unread_buffer_size(stream);
    if (available_size < result_size) {
      log_error(
        "Available bytes to read %d, requested %d",
        available_size,
        result_size);
      error_r = EINVAL;
    }
  }
  if (error_r == 0) {
    *result = io_rf_stream_seek(stream, result_size).data;
  }
  return error_r;
}
