#ifndef PCM_H_
#define PCM_H_

#include "io.h"

struct pcm_spec {
  unsigned int channels_count;
  unsigned int samples_per_sec;
  unsigned short bits_per_sample;
  size_t data_size;
};

inline static size_t
pcm_frame_size(const struct pcm_spec *params) {
  assert(params != NULL);
  assert(params->bits_per_sample % 8 == 0);
  return params->channels_count * params->bits_per_sample / 8;
}

inline static unsigned int
pcm_buffer_time_us(const struct pcm_spec *params, size_t size) {
  assert(params != NULL);
  return size / pcm_frame_size(params)  // frames in buffer
    * 1000 * 1000                       // resolution in us from s
    / params->samples_per_sec;
}

error_t
pcm_validate_wav_content(
  struct io_rf_stream *stream,
  int timeout,
  struct pcm_spec *result,
  struct io_stream_statistics *stats);

#endif