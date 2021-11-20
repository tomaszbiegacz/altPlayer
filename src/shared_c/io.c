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
#include "./io.h"
#include "./log.h"

#define LAST_IO_ERROR errno != 0 ? errno : EIO

static error_t
get_file_size(
  const char *file_path,
  size_t *file_size,
  size_t *read_block_size
) {
  struct stat file_stat;
  if (stat(file_path, &file_stat) != 0)
    return errno;

  if (!S_ISREG(file_stat.st_mode)) {
    log_error("File [%s] is not a regular file.", file_path);
    return EINVAL;
  }

  if (file_stat.st_size == 0) {
    log_error("File [%s] is empty", file_path);
    return EINVAL;
  }

  assert(file_stat.st_size > 0);
  assert(file_stat.st_blksize > 0);

  *file_size = file_stat.st_size;
  *read_block_size = file_stat.st_blksize;
  log_verbose(
    "File [%s] size is %d kB with read block size %d",
    file_path, (*file_size / 1024),
    *read_block_size);
  return 0;
}

static error_t
copy_file_content_into_memory_block(
  int fd,
  size_t read_block_size,
  void* memory_block_start,
  size_t memory_block_size
) {
  error_t error_result = 0;
  size_t currently_read = 0;
  while (currently_read < memory_block_size && error_result == 0) {
    ssize_t read_count = read(
      fd,
      memory_block_start + currently_read,
      read_block_size);

    if (read_count == -1) {
      switch (errno) {
        case EINTR:
          // let's try again
          break;
        default:
          log_error("Got an error when reading file content: %d", errno);
          error_result = LAST_IO_ERROR;
          break;
      }
    } else {
      // we know file size upfront, hence read_count cannot be 0
      assert(read_count > 0);
      currently_read += read_count;
    }
  }

  return error_result;
}

static error_t
copy_file_into_memory_block(
  const char *file_path,
  struct io_memory_block* result,
  size_t read_block_size
) {
  int fd = open(file_path, O_RDONLY);
  if (fd == -1) {
    log_error("Cannot open file [%s]", file_path);
    return errno;
  }

  log_verbose("Reading file %s", file_path);
  error_t result_error = copy_file_content_into_memory_block(
    fd,
    read_block_size,
    result->data,
    result->size);

  if (close(fd) == -1) {
    log_error(
      "Error when closing the file: %s",
      file_path,
      strerror(errno));
  }

  return result_error;
}

error_t
io_memory_block_alloc(
  size_t size,
  struct io_memory_block* result
) {
  assert(result != NULL);
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

error_t
io_memory_block_read_file(
  const char *file_path,
  struct io_memory_block* result
) {
  assert(file_path != NULL);

  size_t file_size;
  size_t read_block_size;
  error_t result_error = get_file_size(
    file_path,
    &file_size,
    &read_block_size);
  if (result_error != 0)
    return result_error;

  result_error = io_memory_block_alloc(file_size, result);
  if (result_error != 0)
    return result_error;

  return copy_file_into_memory_block(file_path, result, read_block_size);
}

void
io_memory_block_free(struct io_memory_block* result) {
  assert(result != NULL);

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

//
// io_fd_cursor
//

static void
io_fd_cursor_close_fd(struct io_fd_cursor* result) {
  if (close(result->fd) == -1) {
    log_error(
      "Error when closing fd_cursor [%s]",
      result->name,
      strerror(errno));
  }
  result->fd = -1;
}

void
io_fd_cursor_free(struct io_fd_cursor* result) {
  assert(result != NULL);

  if (result->name != NULL) {
    log_verbose("Closing fd_cursor [%s]", result->name);
    if (result->fd != -1) {
      io_fd_cursor_close_fd(result);
    }

    io_memory_block_free(&result->buffer);
    result->used_size = 0;

    free(result->name);
    result->name = NULL;
  }
}

error_t
io_fd_cursor_file(
  const char *file_path,
  size_t buffer_size,
  struct io_fd_cursor* result
) {
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
io_fd_cursor_read_non_blocking(
  struct io_fd_cursor* result,
  void* data,
  size_t data_size,
  struct io_fd_cursor_statistics* stats) {
  struct timespec wait_start;
  if (stats != NULL) {
    log_start_timer(&wait_start);
  }
  ssize_t read_count = read(result->fd, data, data_size);
  if (stats != NULL) {
    log_add_elapsed_time(&stats->reading_time, wait_start);
  }

  if (read_count == -1) {
    switch (errno) {
      case EINTR:
        // let's try again
        return io_fd_cursor_read_non_blocking(result, data, data_size, stats);
      default:
        log_error(
          "Got an error when reading content of fd_cursor [%s]",
          result->name);
        return errno;
    }
  }

  if (read_count == 0) {
    log_verbose(
      "Read end of fd_cursor [%s], closing.",
      result->name);
    io_fd_cursor_close_fd(result);
    return 0;
  } else {
    result->used_size += read_count;
  }
  return 0;
}

static error_t
_io_fd_cursor_read(
  struct io_fd_cursor* result,
  int timeout,
  void* data,
  size_t data_size,
  struct io_fd_cursor_statistics* stats) {
  struct pollfd pfd = (struct pollfd) {
    .fd = result->fd,
    .events = POLLIN
  };

  struct timespec wait_start;
  if (stats != NULL) {
    log_start_timer(&wait_start);
  }
  int ready = poll(&pfd, 1, timeout);
  if (stats != NULL) {
    log_add_elapsed_time(&stats->waiting_time, wait_start);
  }

  if (ready == -1) {
    log_error(
      "Poll on fd_cursor [%s] failed with revents %d",
      result->name,
      pfd.revents);
    return LAST_IO_ERROR;
  }

  error_t error_result = 0;
  if (ready > 0 && pfd.revents != 0) {
    if (pfd.revents & POLLIN) {
      error_result = io_fd_cursor_read_non_blocking(
        result, data, data_size, stats);
    }

    if (pfd.revents & POLLHUP) {
      log_verbose(
        "Read end of fd_cursor [%s], closing.",
        result->name);
      io_fd_cursor_close_fd(result);
    }

    if (pfd.revents & POLLERR || pfd.revents & POLLNVAL) {
      log_error(
        "Poll on fd_cursor [%s] finished with revents %d",
        result->name,
        pfd.revents);
      error_result = EIO;
    }
  }
  return error_result;
}

error_t
io_fd_cursor_read(
  struct io_fd_cursor* result,
  int timeout
) {
  assert(result != NULL);
  assert(result->name != NULL);
  assert(result->fd != -1);

  result->used_size = 0;
  return _io_fd_cursor_read(
    result, timeout,
    result->buffer.data, result->buffer.size,
    NULL);
}

error_t
io_fd_cursor_read_full_stats(
  struct io_fd_cursor* result,
  struct io_fd_cursor_statistics* stats) {
  assert(result != NULL);
  assert(result->name != NULL);
  assert(result->fd != -1);

  result->used_size = 0;
  error_t error_result = 0;
  while (
      error_result == 0
      && result->used_size < result->buffer.size
      && result->fd != -1
  ) {
    char* data = result->buffer.data;
    error_result = _io_fd_cursor_read(
      result, -1,
      data  + result->used_size,
      result->buffer.size - result->used_size,
      stats);

    if (stats != NULL) {
      stats->loops_count++;
    }
  }
  return error_result;
}
