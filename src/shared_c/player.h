#ifndef PLAYER_H_
#define PLAYER_H_

#include "pcm.h"

struct player_parameters {
  const char *device_name;
  bool disable_resampling;
  size_t period_size;
  unsigned short periods_per_buffer;
  unsigned short reads_per_period;
};

struct player;

error_t
player_open(
  const struct player_parameters *params,
  struct pcm_decoder *pcm_stream,
  struct player **player);

bool
player_is_eof(struct player *player);

error_t
player_process_once(struct player *player);

struct player_playback_status {
  struct timespec total;
  struct timespec actual;
  struct timespec playback_buffer;
  size_t stream_buffer;
};

error_t
player_get_playback_status(
  struct player *player,
  struct player_playback_status *result);

void
player_release(struct player **player);

#endif
