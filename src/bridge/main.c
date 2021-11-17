#include <argp.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "./log.h"
#include "./player.h"

#define ARGP_KEY_PLAYER_FILE 'f'
#define ARGP_KEY_LOG_VERBOSE 'v'
#define ARGP_KEY_LOG_OUTPUT 1

#define ARGP_GROUP_PLAYER 1
#define ARGP_GROUP_LOG 2

struct bridge_config {
  char* file_path;
};

const char *argp_program_version =
  "altBridge 0.1";

const char *argp_program_bug_address =
  "<https://github.com/tomaszbiegacz/altPlayer/issues>";

static error_t
argp_parser(int key, char *arg, struct argp_state *state);

static void
free_bridge_config(struct bridge_config *config) {
  if (config->file_path != NULL) {
    free(config->file_path);
    config->file_path = NULL;
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
    log_verbose("Starting %s", argp_program_version);
    log_full_system_information();

    if (config.file_path != NULL) {
      log_info("Playing music from [%s]", config.file_path);

      struct io_memory_block pcm_buffer = { 0 };
      struct wav_pcm_content wav_content = { 0 };

      error_result = io_read_file_memory(config.file_path, &pcm_buffer);
      if (error_result == 0)
        error_result = wav_validate_pcm_content(&pcm_buffer, &wav_content);
      if (error_result == 0)
        error_result = player_play_wav_pcm(&wav_content);

      io_free_memory_block(&pcm_buffer);
    } else {
      log_info("Starting player server...");
    }
  }

  log_verbose(
    "Finished with error %d (%s)",
    error_result,
    strerror(error_result));

  free_bridge_config(&config);
  log_free();
  return error_result == 0 ? 0 : -1;
}

static error_t
argp_parser(int key, char *arg, struct argp_state *state) {
  struct bridge_config* const config = state->input;
  switch (key) {
    case ARGP_KEY_PLAYER_FILE:
      config->file_path = strdup(arg);
      if (config->file_path == NULL)
        return ENOMEM;
      else
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
