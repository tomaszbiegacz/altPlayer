#include <argp.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "player.h"

#define ARGP_KEY_PLAYER_FILE 'f'
#define ARGP_KEY_LOG_VERBOSE 'v'
#define ARGP_KEY_LOG_OUTPUT 1
#define ARGP_KEY_LOG_HARDWARE 'h'
#define ARGP_KEY_LOG_PERIOD_SIZE 'p'
#define ARGP_KEY_LOG_PERIOD_COUNT 'c'

#define ARGP_GROUP_PLAYER 1
#define ARGP_GROUP_LOG 2

struct bridge_config {
  char* file_path;
  char* alsa_hadrware;
  size_t period_size;
  size_t period_count;
  int read_timeout;
};

const char *argp_program_version =
  "altBridge 0.1";

const char *argp_program_bug_address =
  "<https://github.com/tomaszbiegacz/altPlayer/issues>";

static error_t
argp_parser(int key, char *arg, struct argp_state *state);

static error_t
bridge_config_defaults(struct bridge_config *config) {
  if (config->alsa_hadrware == NULL) {
    config->alsa_hadrware = strdup("default");
    if (config->alsa_hadrware == NULL)
      return ENOMEM;
  }

  if (config->period_size == 0) {
    config->period_size = 128;
  }
  config->period_size *= 1024;  // in kB

  if (config->period_count == 0) {
    config->period_count = 1024;
  }

  if (config->read_timeout == 0) {
    config->read_timeout = 100;
  }
  return 0;
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

int
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
      .name = "hadware",
      .key = ARGP_KEY_LOG_HARDWARE,
      .arg = "HW",
      .flags = 0,
      .doc = "Alsa hardware name, i.e. 'plughw:CARD=PCH,DEV=0'.",
      .group = ARGP_GROUP_PLAYER
    },
    (struct argp_option) {
      .name = "period_size",
      .key = ARGP_KEY_LOG_PERIOD_SIZE,
      .arg = "SIZE",
      .flags = 0,
      .doc = "Alsa period size in kB, default 64.",
      .group = ARGP_GROUP_PLAYER
    },
    (struct argp_option) {
      .name = "period_count",
      .key = ARGP_KEY_LOG_PERIOD_COUNT,
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
      "Start player in server mode to receive commands from the network or "
      "select a file to play it via ALSA."
      "\n"
      "\nOptions:",
    .children = NULL,
    .help_filter = NULL,
    .argp_domain = NULL
  };

  error_t error_result = argp_parse(&argp_spec, argc, argv, 0, NULL, &config);
  if (error_result == 0) {
    error_result = bridge_config_defaults(&config);
  }
  if (error_result == 0) {
    log_verbose("Starting %s", argp_program_version);
    log_full_system_information();

    if (config.file_path != NULL) {
      log_info("Playing music from [%s]", config.file_path);

      struct player_parameters player_params = (struct player_parameters) {
        .device_name = config.alsa_hadrware,
        .allow_resampling = 1,
        .period_size = config.period_size,
        .periods_per_buffer = config.period_count
      };
      struct io_rf_stream file_stream = { 0 };
      struct pcm_spec stream_spec = { 0 };
      struct io_stream_statistics stream_stats = { 0 };

      error_result = io_rf_stream_open_file(
        config.file_path, config.period_size, &file_stream);
      if (error_result == 0)
        error_result = pcm_validate_wav_content(
          &file_stream, config.read_timeout, &stream_spec, &stream_stats);
      if (error_result == 0)
        error_result = player_pcm_play(
          &player_params,
          config.read_timeout, &stream_spec, &file_stream, &stream_stats);

      io_rf_stream_free(&file_stream);
      log_io_rf_stream_statistics(&stream_stats, "music");
    } else {
      log_info("Starting player server...");
    }
  }

  log_verbose(
    "Finished with error %d (%s)",
    error_result,
    strerror(error_result));

  bridge_config_free(&config);
  log_free();
  return error_result == 0 ? 0 : -1;
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

    case ARGP_KEY_LOG_HARDWARE:
      SAVE_ARG_STRDUP(config->alsa_hadrware);
      return 0;

    case ARGP_KEY_LOG_PERIOD_SIZE:
      SAVE_ARG_UL(config->period_size);
      return 0;

    case ARGP_KEY_LOG_PERIOD_COUNT:
      SAVE_ARG_UL(config->period_count);
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
