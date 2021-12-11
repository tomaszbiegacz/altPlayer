#include <argp.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "flac.h"
#include "log.h"
#include "player.h"
#include "timer.h"

#define ARGP_GROUP_PLAYER 1
#define ARGP_KEY_PLAYER_FILE 'f'
#define ARGP_KEY_PLAYER_BUFFER_SIZE 'b'
#define ARGP_KEY_PLAYER_FILE_FORMAT 't'

#define ARGP_GROUP_ALSA 2
#define ARGP_KEY_ALSA_HARDWARE 'h'
#define ARGP_KEY_ALSA_PERIOD_SIZE 'p'
#define ARGP_KEY_ALSA_PERIOD_COUNT 'c'

#define ARGP_GROUP_LOG 3
#define ARGP_KEY_LOG_VERBOSE 'v'
#define ARGP_KEY_LOG_OUTPUT 1

struct bridge_config {
  char *file_path;
  size_t io_buffer_size;
  enum pcm_format pcm_format;
  char *alsa_hadrware;
  size_t alsa_period_size;
  unsigned int alsa_periods_per_buffer;
};

const char *argp_program_version =
  "altBridge 0.1";

const char *argp_program_bug_address =
  "<https://github.com/tomaszbiegacz/altPlayer/issues>";

static error_t
argp_parser(int key, char *arg, struct argp_state *state);

static error_t
bridge_config_defaults(struct bridge_config *config) {
  error_t error_r = 0;
  if (error_r == 0) {
    if (config->io_buffer_size == 0) {
      config->io_buffer_size = 16;
    }
    config->io_buffer_size *= 1024 * 1024;  // in mB
  }

  if (error_r == 0) {
    if (config->alsa_period_size == 0) {
      config->alsa_period_size = 128;
    }
    config->alsa_period_size *= 1024;  // in kB
  }

  if (error_r == 0 && config->alsa_periods_per_buffer == 0) {
    config->alsa_periods_per_buffer = 64;
  }

  return error_r;
}

static void
bridge_config_free(struct bridge_config *config) {
  if (config->file_path != NULL) {
    free(config->file_path);
    config->file_path = NULL;
  }
  if (config->alsa_hadrware != NULL) {
    free(config->alsa_hadrware);
    config->alsa_hadrware = NULL;
  }
}

static error_t
play(struct player *player) {
  struct player_playback_status status;
  error_t error_r = 0;

  while (error_r == 0 && !player_is_eof(player)) {
    error_r = player_process_once(player);
    if (error_r == 0) {
        error_r = player_get_playback_status(player, &status);
    }
    if (error_r == 0) {
      fprintf(
        stdout,
"Playing %02d:%02d from %02d:%02d (io buffer %ldkb, alsa buffer %dms)       \r",
        timespec_get_minutes(status.actual),
        timespec_get_remaining_seconds(status.actual),
        timespec_get_minutes(status.total),
        timespec_get_remaining_seconds(status.total),
        status.stream_buffer / 1024,
        timespec_miliseconds(status.playback_buffer));
      fflush(stdout);
    }
  }
  printf("\n");
  return error_r;
}

static error_t
play_file(struct bridge_config *config) {
  log_info("Playing music from [%s]", config->file_path);

  struct io_rf_stream file_stream = { 0 };
  struct pcm_decoder *decoder = NULL;
  struct player *player = NULL;
  error_t error_r = 0;
  if (config->pcm_format == 0) {
    error_r = pcm_guess_format(config->file_path, &config->pcm_format);
  }
  if (error_r == 0) {
    size_t max_single_read_size = config->alsa_period_size;
    error_r = io_rf_stream_open_file(
      config->file_path,
      config->io_buffer_size,
      max_single_read_size,
      &file_stream);
  }
  if (error_r == 0) {
    size_t pcm_buffer_size = 2 * config->alsa_period_size;
    switch (config->pcm_format) {
      case pcm_format_wav:
        error_r = pcm_decoder_wav_open(
          &file_stream,
          pcm_buffer_size,
          &decoder);
        break;
      case pcm_format_flac:
        error_r = pcm_decoder_flac_open(
          &file_stream,
          pcm_buffer_size,
          &decoder);
        break;
      default:
        log_error("Unknown format: %d", config->pcm_format);
        error_r = EINVAL;
    }
  }
  if (error_r == 0) {
    struct player_parameters player_params = (struct player_parameters) {
      .hardware_id = config->alsa_hadrware,
      .disable_resampling = 0,
      .period_size = config->alsa_period_size,
      .periods_per_buffer = config->alsa_periods_per_buffer,
      .reads_per_period = 3,
    };
    error_r = player_open(&player_params, decoder, &player);
  }

  if (error_r == 0) {
    error_r = play(player);
  }

  player_release(&player);
  pcm_decoder_decode_release(&decoder);
  io_rf_stream_free(&file_stream);
  return error_r;
}

static error_t
list_sound_cards() {
  struct sound_card_info* card_info = NULL;
  error_t error_r = soundc_get_next_info(&card_info);
  if (card_info != NULL) {
    log_info("Available sound cards:");
    while (error_r == 0 && card_info != NULL) {
      log_info("");
      log_info(
        "Hardware [%s] with driver [%s]",
        soundc_get_hardware_id(card_info),
        soundc_get_driver_name(card_info));
      log_info(
        "Name: %s",
        soundc_get_long_name(card_info));
      log_info(
        "Mixer: %s",
        soundc_get_mixer_name(card_info));
      log_info(
        "Control components: %s",
        soundc_get_components(card_info));

      error_r = soundc_get_next_info(&card_info);
    }
  }  else {
    log_info("No valid sound hardware found, please check /proc/asound/cards");
  }
  return error_r;
}

error_t
main(int argc, char **argv) {
  log_start();
  struct bridge_config config = { 0 };

  const struct argp_option argp_options[] = {
    (struct argp_option) {
      .name = "file",
      .key = ARGP_KEY_PLAYER_FILE,
      .arg = "PATH",
      .flags = 0,
      .doc = "Play file from the given path.",
      .group = ARGP_GROUP_PLAYER
    },
    (struct argp_option) {
      .name = "buffer_size",
      .key = ARGP_KEY_PLAYER_BUFFER_SIZE,
      .arg = "SIZE",
      .flags = 0,
      .doc = "IO buffer size in MB, default 16.",
      .group = ARGP_GROUP_ALSA
    },
    (struct argp_option) {
      .name = "format",
      .key = ARGP_KEY_PLAYER_FILE_FORMAT,
      .arg = "FORMAT",
      .flags = 0,
      .doc = "File format, i.e. wav, flac.",
      .group = ARGP_GROUP_PLAYER
    },
    (struct argp_option) {
      .name = "hadware",
      .key = ARGP_KEY_ALSA_HARDWARE,
      .arg = "HW",
      .flags = 0,
      .doc = "Alsa hardware name, i.e. 'plughw:CARD=PCH,DEV=0'.",
      .group = ARGP_GROUP_ALSA
    },
    (struct argp_option) {
      .name = "period_size",
      .key = ARGP_KEY_ALSA_PERIOD_SIZE,
      .arg = "SIZE",
      .flags = 0,
      .doc = "Alsa period size in kB, default 16.",
      .group = ARGP_GROUP_ALSA
    },
    (struct argp_option) {
      .name = "period_count",
      .key = ARGP_KEY_ALSA_PERIOD_COUNT,
      .arg = "COUNT",
      .flags = 0,
      .doc = "Alsa periods count in buffer, default 1024.",
      .group = ARGP_GROUP_PLAYER
    },
    (struct argp_option) {
      .name = "log-output",
      .key = ARGP_KEY_LOG_OUTPUT,
      .arg = "PATH",
      .flags = 0,
      .doc = "Store diagnostics under file with the given path.",
      .group = ARGP_GROUP_LOG
    },
    (struct argp_option) {
      .name = "verbose",
      .key = ARGP_KEY_LOG_VERBOSE,
      .arg = NULL,
      .flags = 0,
      .doc =
        "Produce verbose output. "
        "If log-output is specified "
        "then store verbose diagnostics only in the file.",
      .group = ARGP_GROUP_LOG
    },
    { 0 }
  };

  const struct argp argp_spec = (struct argp) {
    .options = argp_options,
    .parser = argp_parser,
    .args_doc = NULL,
    .doc =
      "\n"
      "List available sound cards or "
      "select a file to play it via ALSA."
      "\n"
      "\nOptions:",
    .children = NULL,
    .help_filter = NULL,
    .argp_domain = NULL
  };

  error_t error_r = argp_parse(&argp_spec, argc, argv, 0, NULL, &config);
  if (error_r == 0) {
    error_r = bridge_config_defaults(&config);
  }
  if (error_r == 0) {
    log_verbose("Starting %s", argp_program_version);
    log_full_system_information();

    if (config.file_path != NULL) {
      error_r = play_file(&config);
    } else {
      error_r = list_sound_cards();
    }
  }

  log_verbose(
    "Finished with error %d (%s)",
    error_r,
    strerror(error_r));

  bridge_config_free(&config);
  log_free();
  return error_r == 0 ? 0 : -1;
}

#define SAVE_ARG_STRDUP(c) c = strdup(arg);\
  if (c == NULL)\
    return ENOMEM;

#define SAVE_ARG_UL(c) c = strtoul(arg, NULL, 10);\
  if (c == 0) {\
    log_error("Invalid argument: %s", arg);\
    return EINVAL;\
  }

static error_t
argp_parser(int key, char *arg, struct argp_state *state) {
  struct bridge_config* const config = state->input;
  switch (key) {
    case ARGP_KEY_PLAYER_FILE:
      SAVE_ARG_STRDUP(config->file_path);
      return 0;

    case ARGP_KEY_PLAYER_BUFFER_SIZE:
      SAVE_ARG_UL(config->io_buffer_size);
      return 0;

    case ARGP_KEY_PLAYER_FILE_FORMAT:
      if (strcasecmp(arg, "wav") == 0) {
        config->pcm_format = pcm_format_wav;
        return 0;
      } else if (strcasecmp(arg, "flac") == 0) {
        config->pcm_format = pcm_format_flac;
        return 0;
      } else {
        log_error("Unknown file format: %s", arg);
        return EINVAL;
      }

    case ARGP_KEY_ALSA_HARDWARE:
      if (soundc_is_valid_hardware_id(arg)) {
        SAVE_ARG_STRDUP(config->alsa_hadrware);
        return 0;
      } else {
        log_error("Unknown ALSA hardware: %s", arg);
        return EINVAL;
      }

    case ARGP_KEY_ALSA_PERIOD_SIZE:
      SAVE_ARG_UL(config->alsa_period_size);
      return 0;

    case ARGP_KEY_ALSA_PERIOD_COUNT:
      SAVE_ARG_UL(config->alsa_periods_per_buffer);
      return 0;

    case ARGP_KEY_LOG_VERBOSE:
      log_set_verbose(true);
      return 0;

    case ARGP_KEY_LOG_OUTPUT:
      return log_open_output_st(arg);

    case ARGP_KEY_ARG:
      return ARGP_ERR_UNKNOWN;

    case ARGP_KEY_ARGS:
      log_error("Unknown CLI argument: %s", arg);
      argp_usage(state);
      return EINVAL;

    default:
      return 0;
  }
}
