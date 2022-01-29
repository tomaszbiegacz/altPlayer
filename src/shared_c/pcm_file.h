#pragma once
#include "shrdef.h"

enum pcm_file_format {
  pcm_file_format_wav      = 1,
  pcm_file_format_flac     = 2,
};

error_t
pcm_file_format_guess(
  const char *file_name,
  enum pcm_file_format *format);
