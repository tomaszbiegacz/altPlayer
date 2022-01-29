#pragma once
#include "pcm_decoder.h"

/**
 * @brief Player parameters used for setting it up
 *
 */
struct alsa_player_parameters {
  const char *hardware_id;
  bool disable_resampling;
  int blocking_read_timeout;
};

/**
 * @brief Player handle type
 *
 */
struct alsa_player;

/**
 * @brief Player status like total and actual time playback time
 *
 */
struct alsa_player_playback_status {
  bool is_complete;
  struct timespec position;
  size_t stream_buffer;
  struct timespec playback_buffer;
  size_t playback_buffer_frames;
};


//
// Setup
//

error_t
alsa_player_open(
  const struct alsa_player_parameters *params,
  struct alsa_player **player);

void
alsa_player_release(struct alsa_player **player);


//
// Query
//

struct timespec
alsa_player_get_stream_time(const struct alsa_player *player);

error_t
alsa_player_get_playback_status(
  const struct alsa_player *player,
  struct alsa_player_playback_status *result);

//
// Command
//

error_t
alsa_player_start(
  struct alsa_player *player,
  struct pcm_decoder *pcm_stream);

error_t
alsa_player_stop(struct alsa_player *player);

error_t
alsa_player_process_once(
  struct alsa_player *player,
  bool *has_been_waiting);
