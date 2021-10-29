#include <argp.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "./diagnostics.h"
#include "./player.h"

const char *argp_program_version =
  "altBridge 0.1";

const char *argp_program_bug_address =
  "<https://github.com/tomaszbiegacz/player/issues>";

static const char argp_doc[] =
  "\n"
  "Start player in server mode to receive commands from the network or "
  "select a file to play it via ALSA."
  "\n"
  "\nOptions:";

#define ARGP_KEY_PLAYER_FILE 'f'
#define ARGP_KEY_LOG_VERBOSE 'v'
#define ARGP_KEY_LOG_OUTPUT 1

#define ARGP_GROUP_PLAYER 1
#define ARGP_GROUP_LOG 2

static const struct argp_option argp_options[] = {
  (struct argp_option) {
    .name = "file",
    .key = ARGP_KEY_PLAYER_FILE,
    .arg = "PATH",
    .flags = 0,
    .doc = "Play file from given path.",
    .group = ARGP_GROUP_PLAYER
  },
  (struct argp_option) {
    .name = "log-output",
    .key = ARGP_KEY_LOG_OUTPUT,
    .arg = "PATH",
    .flags = 0,
    .doc = "Store diagnostics under file with a given path.",
    .group = ARGP_GROUP_LOG
  },
  (struct argp_option) {
    .name = "verbose",
    .key = ARGP_KEY_LOG_VERBOSE,
    .arg = NULL,
    .flags = 0,
    .doc =
      "Produce verbose output. "
      "If log-output is specified store verbose diagnostics only in the file.",
    .group = ARGP_GROUP_LOG
  },
  { 0 }
};

static error_t
argp_parser(int key, char *arg, struct argp_state *state);

static const struct argp argp_spec = (struct argp) {
    .options = argp_options,
    .parser = argp_parser,
    .args_doc = NULL,
    .doc = argp_doc,
    .children = NULL,
    .help_filter = NULL,
    .argp_domain = NULL
};

#define EARGUMENTS 2000

static void
log_free();

int
main(int argc, char **argv) {
    struct player_config config = (struct player_config) {
      .file_path = NULL
    };

    error_t result = argp_parse(&argp_spec, argc, argv, 0, NULL, &config);
    log_verbose("Here we go!\n");
    if (result == 0) {
      if (config.file_path != NULL) {
        log_info("Playing music from [%s]\n", config.file_path);
      } else {
        log_info("Starting player server...\n");
      }
    } else {
      if (result != EALREADY)
          perror(strerror(result));
    }

    log_free();
    return result == 0 ? 0 : -1;
}

static bool log_is_verbose = false;
static FILE* log_output_st = NULL;

static error_t
log_open_output(const char *path) {
  assert(log_output_st == NULL);

  log_output_st = fopen(path, "a");
  if (log_output_st == NULL) {
    fprintf(
      stderr,
      "Error when opening log output [%s]: %s",
      path,
      strerror(errno));
  }

  return 0;
}

static void
log_free() {
  if (log_output_st != NULL) {
    error_t result = fclose(log_output_st);
    if (result != 0) {
      fprintf(stderr, "Error when closing log output: %s\n", strerror(result));
    }
    log_output_st = NULL;
  }
}

static error_t
argp_parser(int key, char *arg, struct argp_state *state) {
  struct player_config* const config = state->input;
  switch (key) {
    case ARGP_KEY_PLAYER_FILE:
      config->file_path = arg;
      return 0;

    case ARGP_KEY_LOG_VERBOSE:
      log_is_verbose = true;
      return 0;

    case ARGP_KEY_LOG_OUTPUT:
      return log_open_output(arg);

    case ARGP_KEY_ARG:
      return ARGP_ERR_UNKNOWN;

    case ARGP_KEY_ARGS:
      fprintf(stderr, "Unknown option: %s\n", arg);
      argp_usage(state);
      return EARGUMENTS;
  }
  return 0;
}

static void
print_date_time(FILE* st) {
  time_t now;
  struct tm local_now;

  time(&now);
  if (localtime_r(&now, &local_now) != NULL) {
    char str_now[256];
    size_t len = strftime(str_now, sizeof(str_now), "%F %H:%M:%S", &local_now);
    if (len > 0) {
      fprintf(st, "[%s] ", str_now);
    }
  }
}

void
log_verbose(const char *format, ...) {
  if (log_is_verbose) {
    FILE* st = log_output_st != NULL ? log_output_st : stderr;
    if (log_output_st != NULL) {
      print_date_time(log_output_st);
    }

    va_list args;
    va_start(args, format);
    error_t result = vfprintf(st, format, args);
    if (result < 0) {
      fprintf(stderr, "Error when writing verbose logs: %s\n", strerror(errno));
    }
    va_end(args);
  }
}

void
log_info(const char *format, ...) {
  va_list args;
  va_start(args, format);

  vprintf(format, args);
  if (log_output_st != NULL) {
    print_date_time(log_output_st);
    error_t result = vfprintf(log_output_st, format, args);
    if (result < 0) {
      fprintf(
        stderr,
        "Error when writing logs to output file: %s\n",
        strerror(errno));
    }
  }

  va_end(args);
}
