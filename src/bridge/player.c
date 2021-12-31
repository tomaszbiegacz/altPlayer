#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include "main.h"
#include "pcm_decoder_flac.h"
#include "pcm_decoder_wav.h"
#include "timer.h"

static error_t
player_open(
  const struct bridge_config *config,
  struct alsa_player **player) {
    struct alsa_player_parameters player_params;
    player_params.hardware_id = config->alsa_hadrware;
    player_params.disable_resampling = !config->alsa_resampling;
    return alsa_player_open(&player_params, player);
  }

static error_t
open_decoder_from_file(
  const struct bridge_config *config,
  const char* file_path,
  struct pcm_decoder **decoder) {
    error_t error_r = 0;
    struct io_rf_stream *file_stream = NULL;

    enum pcm_file_format format;
    if (config->pcm_format == 0) {
      error_r = pcm_file_format_guess(file_path, &format);
    } else {
      format = config->pcm_format;
    }

    if (error_r == 0) {
      error_r = io_rf_stream_open_file(
        config->file_path,
        config->io_buffer_size,
        4096,
        &file_stream);
    }
    if (error_r == 0) {
      switch (format) {
        case pcm_file_format_wav:
          error_r = pcm_decoder_wav_open(
            file_stream,
            decoder);
          break;
        case pcm_file_format_flac:
          error_r = pcm_decoder_flac_open(
            file_stream,
            decoder);
          break;
        default:
          log_error("Unknown format: %d", config->pcm_format);
          error_r = EINVAL;
      }
    }

    if (error_r != 0) {
      io_rf_stream_release(&file_stream);
    }
    return error_r;
  }

static void
player_print_status(
  const struct timespec stream_time,
  const struct alsa_player_playback_status status) {
    fprintf(
      stdout,
"Playing %02d:%02d.%03d from %02d:%02d.%03d (io buffer %ldkb, alsa buffer %ldms)       \r", // NOLINT
      timespec_get_minutes(status.position),
      timespec_get_seconds_remaining(status.position),
      timespec_get_miliseconds_remaining(status.position),
      timespec_get_minutes(stream_time),
      timespec_get_seconds_remaining(stream_time),
      timespec_get_miliseconds_remaining(stream_time),
      status.stream_buffer / 1024,
      timespec_get_miliseconds(status.playback_buffer));

    fflush(stdout);
  }

static error_t
play_stream(
  struct alsa_player *player,
  struct pcm_decoder *decoder) {
    struct timespec stream_time;
    struct alsa_player_playback_status status;
    error_t error_r;

    error_r = alsa_player_start(player, decoder);
    if (error_r == 0) {
      stream_time = alsa_player_get_stream_time(player);
      error_r = alsa_player_get_playback_status(player, &status);
    }
    if (error_r == 0) {
      player_print_status(stream_time, status);
    }
    while (error_r == 0 && !status.is_complete) {
      bool has_been_waiting = false;
      error_r = alsa_player_process_once(player, &has_been_waiting);
      if (error_r == 0) {
        error_r = alsa_player_get_playback_status(player, &status);
      }
      if (error_r == 0) {
          player_print_status(stream_time, status);
          if (!has_been_waiting) {
            usleep(100 * 1000);
          }
      }
    }
    printf("\n");

    if (error_r == 0) {
      error_r = alsa_player_stop(player);
    }
    return error_r;
  }

error_t
play_file(struct bridge_config *config) {
  log_info("Playing music from [%s]", config->file_path);

  struct alsa_player *player = NULL;
  struct pcm_decoder *decoder = NULL;
  error_t error_r = 0;

  error_r = player_open(config, &player);

  if (error_r == 0) {
    error_r = open_decoder_from_file(config, config->file_path, &decoder);
  }
  if (error_r == 0) {
    error_r = play_stream(player, decoder);
  }

  alsa_player_release(&player);
  return error_r;
}
