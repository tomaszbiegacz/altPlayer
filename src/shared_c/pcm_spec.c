#include "pcm_spec.h"

struct timespec
pcm_spec_get_time_for_frames(
  const struct pcm_spec *params,
  size_t frames_count) {
    assert(params != NULL);
    struct timespec result = {0};
    result.tv_sec = frames_count / params->samples_per_sec;
    result.tv_nsec = 1000l * 1000l * 1000l  // nano in second
      * (frames_count - result.tv_sec * params->samples_per_sec)
      / params->samples_per_sec;
    return result;
  }

size_t
pcm_spec_get_frames_count_for_time(
  const struct pcm_spec *params,
  size_t time_ms) {
    assert(params != NULL);
    return params->samples_per_sec * time_ms / 1000;
  }
