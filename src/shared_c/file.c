#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include "file.h"
#include "log.h"


const char*
file_name_get_ext(const char *file_name) {
  assert(file_name != NULL);
  const char *dot = strrchr(file_name, '.');
  if (dot == NULL || dot == file_name)
    return "";
  else
    return dot + 1;
}

error_t
file_open_read(const char *path, int *fd) {
  assert(path != NULL);
  assert(fd != NULL);

  error_t error_r = 0;
  *fd = open(path, O_RDONLY | O_NONBLOCK);
  if (*fd == -1) {
    error_r = errno;
    log_error(
      "Cannot open file [%s] due to %s",
      path,
      strerror(error_r));
  }
  return error_r;
}

error_t
file_read(
  const char *path,
  int fd,
  void *buffer,
  size_t *size) {
    assert(path != NULL);
    assert(fd >= 0);
    assert(buffer != NULL);
    assert(size != NULL);
    assert(*size > 0);

    ssize_t read_count = read(
      fd,
      buffer,
      *size);

    if (read_count < 0) {
      if (errno == EINTR) {
        // let's try again
        return file_read(path, fd, buffer, size);
      } else {
        error_t error_r = errno;
        log_error(
          "Reading from [%s] failed with %s",
          path,
          strerror(error_r));
        return error_r;
      }
    } else {
      *size = read_count;
      return 0;
    }
  }

void
file_close(const char *path, int *fd) {
  assert(path != NULL);
  assert(fd != NULL);

  if (*fd != -1) {
    if (close(*fd) == -1) {
      log_error(
        "Error when closing file [%s] due to %s",
        path,
        strerror(errno));
    } else {
      log_verbose("Closed file [%s]", path);
    }
    *fd = -1;
  }
}
