#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "./io.h"
#include "./log.h"

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
        case EAGAIN:
          read_block_size = read_block_size / 2;
          if (read_block_size == 0) {
            log_error("Read block size is zero, stoping");
            error_result = EIO;
          } else {
            log_verbose(
              "Read block size is too large, decreasing to %d",
              read_block_size);
          }
        case EINTR:
          // let's try again
          break;
        default:
          log_error("Got an error when reading file content");
          error_result = errno;
          break;
      }
    } else {
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
io_alloc(
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

  result->data = mem_range;
  result->size = size;
  return 0;
}

error_t
io_read_file_memory(
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

  result_error = io_alloc(file_size, result);
  if (result_error != 0)
    return result_error;

  return copy_file_into_memory_block(file_path, result, read_block_size);
}

void
io_free_memory_block(struct io_memory_block* result) {
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
