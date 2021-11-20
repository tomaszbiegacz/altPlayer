#ifndef _H_WAV
#define _H_WAV

#include "io.h"

struct wav_pcm_content {
  unsigned int channels_count;
  unsigned int samples_per_sec;
  unsigned short bits_per_sample;
  struct io_memory_block pcm;
};

error_t
wav_validate_pcm_content(
  struct io_memory_block *content,
  struct wav_pcm_content *result);

#endif
