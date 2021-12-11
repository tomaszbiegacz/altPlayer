#ifndef PLAYER_H_
#define PLAYER_H_

#include "pcm.h"

struct sound_card_info;

error_t
soundc_get_next_info(struct sound_card_info **info);

const char*
soundc_get_hardware_id(const struct sound_card_info *info);

const char*
soundc_get_long_name(const struct sound_card_info *info);

const char*
soundc_get_driver_name(const struct sound_card_info *info);

const char*
soundc_get_mixer_name(const struct sound_card_info *info);

const char*
soundc_get_components(const struct sound_card_info *info);

void
soundc_release(struct sound_card_info **info);

bool
soundc_is_valid_hardware_id(const char *hardware_id);

/**
 * @brief Player parameters used for setting it up
 *
 */
struct player_parameters {
  const char *hardware_id;
  bool disable_resampling;
  size_t period_size;
  unsigned short periods_per_buffer;
  unsigned short reads_per_period;
};

/**
 * @brief Player handle type
 *
 */
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

/**
 * @brief Player status like total and actual time playback time
 *
 */
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
