#ifndef _H_WAV
#define _H_WAV

#include "./io.h"

struct wav_pcm_stereo_content {
  unsigned int samples_per_sec;
  unsigned short bits_per_sample;
  size_t data_size;
  const void* data;
};

error_t
validate_wav_pcm_stereo_content(
  const struct io_memory_block *content,
  struct wav_pcm_stereo_content *result);

#endif
