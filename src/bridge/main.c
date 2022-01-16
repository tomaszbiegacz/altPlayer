#include <argp.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alsa_soundc.h"
#include "main.h"
#include "log.h"

#define ARGP_GROUP_PLAYER 1
#define ARGP_KEY_PLAYER_FILE 'f'
#define ARGP_KEY_PLAYER_BUFFER_SIZE 'b'
#define ARGP_KEY_PLAYER_FILE_FORMAT 't'

#define ARGP_GROUP_ALSA 2
#define ARGP_KEY_ALSA_HARDWARE 'h'
#define ARGP_KEY_ALSA_RESAMPLING 'r'

#define ARGP_GROUP_LOG 3
#define ARGP_KEY_LOG_VERBOSE 'v'
#define ARGP_KEY_LOG_OUTPUT 1

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

error_t
main10(int argc, char **argv) {
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
      .name = "resampling",
      .key = ARGP_KEY_ALSA_RESAMPLING,
      .arg = NULL,
      .flags = 0,
      .doc = "Enable resampling, if supported for hardware.",
      .group = ARGP_GROUP_ALSA
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
    log_system_information();

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
  log_global_release();
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
        config->pcm_format = pcm_file_format_wav;
        return 0;
      } else if (strcasecmp(arg, "flac") == 0) {
        config->pcm_format = pcm_file_format_flac;
        return 0;
      } else {
        log_error("Unknown file format: %s", arg);
        return EINVAL;
      }

    case ARGP_KEY_ALSA_HARDWARE:
      if (alsa_soundc_is_valid_hardware_id(arg)) {
        SAVE_ARG_STRDUP(config->alsa_hadrware);
        return 0;
      } else {
        log_error("Unknown ALSA hardware: %s", arg);
        return EINVAL;
      }

    case ARGP_KEY_ALSA_RESAMPLING:
      config->alsa_resampling = true;
      return 0;

    case ARGP_KEY_LOG_VERBOSE:
      log_set_verbose(true);
      return 0;

    case ARGP_KEY_LOG_OUTPUT:
      return log_append_to_file(arg);

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
