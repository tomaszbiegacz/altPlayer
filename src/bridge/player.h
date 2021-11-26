#ifndef PLAYER_H_
#define PLAYER_H_

#include "pcm.h"

struct player_parameters {
  char *device_name;
  bool allow_resampling;
  size_t period_size;
  unsigned short periods_per_buffer;
};

error_t
player_pcm_play(
  const struct player_parameters *params,
  int timeout,
  const struct pcm_spec *stream_spec,
  struct io_rf_stream *stream,
  struct io_stream_statistics *stats);

#endif
