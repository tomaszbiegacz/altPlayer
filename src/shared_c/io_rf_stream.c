#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "io_rf_stream.h"
#include "log.h"
#include "timer.h"

#define LAST_IO_ERROR errno != 0 ? errno : EIO

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
  struct io_rf_stream **handle) {
    assert(handle != NULL);
    error_t error_r = 0;

    struct io_rf_stream *result =
      (struct io_rf_stream*)calloc(1, sizeof(struct io_rf_stream));
    if (result == NULL) {
      log_error("PLAYER: Cannot allocate memory for io_rf_stream");
      error_r = ENOMEM;
    }

    if (error_r == 0) {
      result->fd = open(file_path, O_RDONLY | O_NONBLOCK);
      if (result->fd == -1) {
        log_error("Cannot open file [%s]", file_path);
        error_r = errno;
      }
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
      if (result != NULL) {
        assert(result->name == NULL);
        if (result->fd != -1) {
          close(result->fd);
          result->fd = -1;
        }
        io_buffer_free(&result->buffer);
        free(result);
      }
    } else {
      result->buffer_max_single_read_size = buffer_max_single_read_size;
      if (log_is_verbose()) {
        io_rf_stream_enable_stats(result);
      }
      *handle = result;
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
      "EOF of rf_stream [%s], closing  ",
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
    assert(src != NULL);
    assert(item_size > 0);
    assert(item_size <= io_buffer_get_allocated_size(&src->buffer));
    assert(dest != NULL);
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
      timer_start(&wait_start);
    }
    int ready = poll(&pfd, 1, poll_timeout);
    if (src->stats != NULL) {
      timer_add_elapsed(&src->stats->waiting_time, wait_start);
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
io_rf_stream_release(struct io_rf_stream **handle) {
  assert(handle != NULL);
  struct io_rf_stream *result = *handle;
  if (result != NULL) {
    if (result->stats != NULL) {
      log_verbose(
        "Stream %s stats: waiting %dms, reading %dms",
        result->name,
        timespec_get_miliseconds(result->stats->waiting_time),
        timespec_get_miliseconds(result->stats->reading_time));

      free(result->stats);
      result->stats = NULL;
    }

    log_verbose("Closing rf_stream [%s]", result->name);
    free(result->name);

    if (result->fd != -1) {
      io_rf_stream_close_fd(result);
    }
    io_buffer_free(&result->buffer);

    free(result);
    *handle = NULL;
  }
}
