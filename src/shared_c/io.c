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

const char*
get_filename_ext(const char *file_name) {
  assert(file_name != NULL);
  const char *dot = strrchr(file_name, '.');
  if (dot == NULL || dot == file_name)
    return "";
  else
    return dot + 1;
}

error_t
io_buffer_alloc(
  size_t size,
  struct io_buffer *result) {
    assert(result != NULL);
    assert(result->data == NULL);

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
    result->size_allocated = size;
    result->size_used = 0;
    result->start_offset = 0;
    result->data = mem_range;
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
      log_timer_start(&wait_start);
    }
    size_t read_size = min_size_t(
      max_read_size,
      dest->size_allocated - dest->size_used);
    ssize_t read_count = read(
      fd,
      io_buffer_data_start_write(dest),
      read_size);
    if (stats != NULL) {
      log_add_elapsed_time(&stats->reading_time, wait_start);
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

static void
io_rf_stream_enable_stats(struct io_rf_stream *src)  {
  assert(src != NULL);
  if (src->stats == NULL) {
    src->stats = calloc(1, sizeof(struct io_stream_statistics));
    if (src->stats == NULL) {
      log_error("Out of memory for allocating rf_stream stats");
    }
  }
}

error_t
io_rf_stream_open_file(
  const char *file_path,
  size_t buffer_size,
  size_t buffer_max_single_read_size,
  struct io_rf_stream *result) {
    assert(result != NULL);
    assert(result->name == NULL);

    error_t error_r = 0;
    result->fd = open(file_path, O_RDONLY | O_NONBLOCK);
    if (result->fd == -1) {
      log_error("Cannot open file [%s]", file_path);
      error_r = errno;
    }

    if (error_r == 0) {
      error_r = io_buffer_alloc(
        buffer_size,
        &result->buffer);
    }

    if (error_r == 0) {
      result->name = strdup(file_path);
      if (result->name == NULL) {
        error_r = ENOMEM;
      }
    }

    if (error_r != 0) {
      assert(result->name == NULL);
      if (result->fd != -1) {
        close(result->fd);
        result->fd = -1;
      }
      io_buffer_free(&result->buffer);
    } else {
      result->buffer_max_single_read_size = buffer_max_single_read_size;
      if (log_is_verbose()) {
        io_rf_stream_enable_stats(result);
      }
    }
    return error_r;
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

static error_t
io_rf_stream_read_once(struct io_rf_stream *src) {
  bool is_eof;
  error_t error_r = io_buffer_write_from_read(
    &src->buffer,
    src->buffer_max_single_read_size,
    src->fd,
    &is_eof,
    src->stats);

  if (error_r == 0 && is_eof) {
    log_verbose(
      "EOF of rf_stream [%s], closing",
      src->name);
    io_rf_stream_close_fd(src);
  } else if (error_r != 0) {
    log_error(
      "Got error when reading from rf_stream [%s]: %d",
      src->name, error_r);
  }

  return error_r;
}

error_t
io_rf_stream_read(
  struct io_rf_stream *src,
  size_t item_size,
  void **dest) {
    assert(item_size > 0);
    assert(item_size <= io_rf_stream_get_allocated_buffer_size(src));
    error_t error_r = 0;
    while (
      error_r == 0
      && !io_rf_stream_is_eof(src)
      && io_buffer_get_unread_size(&src->buffer) < item_size) {
        error_r = io_rf_stream_read_once(src);
      }

    if (error_r == 0) {
      bool result = io_buffer_try_read(&src->buffer, item_size, dest);
      if (!result) {
        log_error(
          "Cannot read block of size %d from rf_stream [%s]",
          item_size, src->name);
        error_r = EIO;
      }
    }

    return error_r;
  }

error_t
io_rf_stream_read_with_poll(
  struct io_rf_stream *src,
  int poll_timeout) {
    assert(!io_rf_stream_is_eof(src));
    struct pollfd pfd = (struct pollfd) {
      .fd = src->fd,
      .events = POLLIN
    };

    struct timespec wait_start;
    if (src->stats != NULL) {
      log_timer_start(&wait_start);
    }
    int ready = poll(&pfd, 1, poll_timeout);
    if (src->stats != NULL) {
      log_add_elapsed_time(&src->stats->waiting_time, wait_start);
    }

    if (ready == -1) {
      log_error(
        "Poll on rf_stream [%s] failed with revents %d",
        src->name,
        pfd.revents);
      return LAST_IO_ERROR;
    }

    error_t error_r = EAGAIN;
    if (ready > 0 && pfd.revents != 0) {
      if (pfd.revents & POLLIN) {
          error_r = io_rf_stream_read_once(src);
      } else if (pfd.revents & POLLHUP) {
          log_verbose(
            "rf_stream [%s] has been shutdown, closing",
            src->name);
          io_rf_stream_close_fd(src);
          error_r = 0;
      } else {
        log_error(
          "Poll on rf_stream [%s] finished with revents %d",
          src->name,
          pfd.revents);
        error_r = EIO;
      }
    }
    return error_r;
  }

void
io_rf_stream_free(struct io_rf_stream *result) {
  assert(result != NULL);

  if (result->stats != NULL) {
    log_verbose(
      "Stream %s stats: waiting %d.%03ds, reading %d.%03ds",
      result->name,
      result->stats->waiting_time.tv_sec,
      result->stats->waiting_time.tv_nsec / 1000000,
      result->stats->reading_time.tv_sec,
      result->stats->reading_time.tv_nsec / 1000000);

    free(result->stats);
    result->stats = NULL;
  }

  if (result->name != NULL) {
    log_verbose("Closing rf_stream [%s]", result->name);
    if (result->fd != -1) {
      io_rf_stream_close_fd(result);
    }

    io_buffer_free(&result->buffer);

    free(result->name);
    result->name = NULL;
  }
}
