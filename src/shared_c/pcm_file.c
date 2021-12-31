#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "filename.h"
#include "pcm_file.h"

error_t
pcm_file_format_guess(
  const char *file_name,
  enum pcm_file_format *format) {
    assert(format != NULL);
    const char *ext = filename_get_ext(file_name);

    if (strcasecmp(ext, "wav") == 0) {
      *format = pcm_file_format_wav;
      return 0;
    }

    if (strcasecmp(ext, "flac") == 0) {
      *format = pcm_file_format_flac;
      return 0;
    }

    return EINVAL;
  }
