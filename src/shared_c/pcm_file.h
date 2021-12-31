#ifndef PLAYER_PCM_FILE_H_
#define PLAYER_PCM_FILE_H_

#include "shrdef.h"

enum pcm_file_format {
  pcm_file_format_wav      = 1,
  pcm_file_format_flac     = 2,
};

error_t
pcm_file_format_guess(
  const char *file_name,
  enum pcm_file_format *format);

#endif
