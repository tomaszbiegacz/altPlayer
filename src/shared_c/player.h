#ifndef PLAYER_H_
#define PLAYER_H_

#include "pcm.h"

struct player_parameters {
  const char *device_name;
  bool disable_resampling;
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

void
player_release(struct player **player);

#endif
