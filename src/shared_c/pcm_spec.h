#pragma once
#include <time.h>
#include "log.h"

//
// sample - single value in PCM format
// frame - set of samples for each channel
//

struct pcm_spec {
  unsigned short channels_count;
  unsigned int samples_per_sec;
  unsigned short bytes_per_sample;
  bool is_big_endian;
  bool is_signed;
  size_t frames_count;
};


//
// Query
//

inline static size_t
pcm_spec_get_frames_count(const struct pcm_spec *params) {
  assert(params != NULL);
  return params->frames_count;
}

inline static size_t
pcm_spec_get_bytes_per_sample(const struct pcm_spec *params) {
  assert(params != NULL);
  return params->bytes_per_sample;
}

inline static size_t
pcm_spec_get_bits_per_sample(const struct pcm_spec *params) {
  assert(params != NULL);
  return params->bytes_per_sample * 8;
}

inline static unsigned short
pcm_spec_get_channels_count(const struct pcm_spec *params) {
  assert(params != NULL);
  return params->channels_count;
}

inline static unsigned int
pcm_spec_get_samples_per_sec(const struct pcm_spec *params) {
  assert(params != NULL);
  return params->samples_per_sec;
}

inline static bool
pcm_spec_is_signed(const struct pcm_spec *params) {
  assert(params != NULL);
  return params->is_signed;
}

inline static bool
pcm_spec_is_big_endian(const struct pcm_spec *params) {
  assert(params != NULL);
  return params->is_big_endian;
}

inline static size_t
pcm_spec_get_frame_size(const struct pcm_spec *params) {
  assert(params != NULL);
  return params->channels_count * params->bytes_per_sample;
}

inline static struct timespec
pcm_spec_get_time_for_frames(
  const struct pcm_spec *params,
  size_t frames_count) {
    assert(params != NULL);
    struct timespec result;
    result.tv_sec = frames_count / params->samples_per_sec;
    result.tv_nsec = 1000l * 1000l * 1000l  // nano in second
      * (frames_count - result.tv_sec * params->samples_per_sec)
      / params->samples_per_sec;
    return result;
  }

inline static struct timespec
pcm_spec_get_time(const struct pcm_spec *params) {
  assert(params != NULL);
  return pcm_spec_get_time_for_frames(params, params->frames_count);
}

inline static size_t
pcm_spec_get_frames_count_for_time(
  const struct pcm_spec *params,
  size_t time_ms) {
    assert(params != NULL);
    return params->samples_per_sec * time_ms / 1000;
  }

inline static void
pcm_spec_log(const char *block_name, const struct pcm_spec *params) {
  assert(params != NULL);
  if (log_is_verbose()) {
    log_verbose(
"%s: PCM %d ch, %d Hz, bitrate %d, %s %s, %d frames",
      block_name,
      pcm_spec_get_channels_count(params),
      pcm_spec_get_samples_per_sec(params),
      pcm_spec_get_bits_per_sample(params),
      pcm_spec_is_signed(params) ? "signed" : "unsigned",
      pcm_spec_is_big_endian(params) ? "big endian" : "little endian",
      pcm_spec_get_frames_count(params));
  }
}
