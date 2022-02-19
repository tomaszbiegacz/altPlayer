#pragma once
#include "shrdef.h"

//
// File name utilities
//

const char*
file_name_get_ext(const char *file_name);

//
// File IO
//

error_t
file_open_read(const char *path, int *fd);

error_t
file_open_write(const char *path, int *fd);

error_t
file_read(
  const char *path,
  int fd,
  void *buffer,
  size_t *size);

error_t
file_write(
  const char *path,
  int fd,
  const void *buffer,
  size_t *size);

void
file_close(const char *path, int *fd);
