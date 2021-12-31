#include <assert.h>
#include <string.h>
#include "filename.h"

const char*
filename_get_ext(const char *file_name) {
  assert(file_name != NULL);
  const char *dot = strrchr(file_name, '.');
  if (dot == NULL || dot == file_name)
    return "";
  else
    return dot + 1;
}
