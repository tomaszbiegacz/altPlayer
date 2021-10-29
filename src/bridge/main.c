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
  "<https://github.com/tomaszbiegacz/altPlayer/issues>";

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

// there is some issue with arguments parsing
#define EARGUMENTS 2000

// session id selected at the program start
static char log_session_id[10];

static void
log_free();

int
main(int argc, char **argv) {
  unsigned int rand_seed;
  snprintf(log_session_id, sizeof(log_session_id), "%x", rand_r(&rand_seed));

  struct player_config config = (struct player_config) {
    .file_path = NULL
  };

  error_t result = argp_parse(&argp_spec, argc, argv, 0, NULL, &config);
  if (result == 0) {
    if (config.file_path != NULL) {
      log_info("Playing music from [%s]", config.file_path);
    } else {
      log_info("Starting player server...");
    }
  } else {
    if (result != EARGUMENTS)
        log_error(strerror(result));
  }

  log_verbose("Finished with error %d", result);
  log_free();
  player_free_config(&config);
  return result == 0 ? 0 : -1;
}

static bool log_is_verbose = false;
static FILE* log_output_st = NULL;

static error_t
log_open_output(const char *path) {
  assert(log_output_st == NULL);

  log_output_st = fopen(path, "a");
  if (log_output_st == NULL) {
    // ignore failure
    fprintf(
      stderr,
      "Error when opening log output [%s]: %s\n",
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
      // ignore failure
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
      config->file_path = strdup(arg);
      if (config->file_path == NULL)
        return ENOMEM;
      break;

    case ARGP_KEY_LOG_VERBOSE:
      log_is_verbose = true;
      break;

    case ARGP_KEY_LOG_OUTPUT:
      return log_open_output(arg);

    case ARGP_KEY_ARG:
      return ARGP_ERR_UNKNOWN;

    case ARGP_KEY_ARGS:
      log_error("Unknown CLI argument: %s", arg);
      argp_usage(state);
      return EARGUMENTS;
  }
  return 0;
}

/**
 * Logging utilities
 */

static void
log_print_beginning(FILE* st) {
  assert(st != NULL);

  time_t now;
  struct tm local_now;

  // this cannot fail
  time(&now);

  if (localtime_r(&now, &local_now) != NULL) {
    char str_now[256];
    size_t len = strftime(str_now, sizeof(str_now), "%F %H:%M:%S", &local_now);
    if (len > 0) {
      // ignore failure
      fputs("[", st);
      fputs(str_now, st);
      fputs(", ", st);
      fputs(log_session_id, st);
      fputs("] ", st);
      return;
    }
  }

  // there is some issue with date&time formatting
  // ignore failure
  fputs("[", st);
  fputs(log_session_id, st);
  fputs("] ", st);
}

static void
log_write_output(const char *format, va_list args) {
  log_print_beginning(log_output_st);
  error_t result = vfprintf(log_output_st, format, args);
  if (result < 0) {
    // ignore failure
    fputs("Error when writing logs to output file: ", stderr);
    fputs(strerror(errno), stderr);
    fputs("\n", stderr);
  } else {
    // ignore failure
    putc('\n', log_output_st);
  }
}

void
log_verbose(const char *format, ...) {
  if (log_is_verbose) {
    va_list args;
    va_start(args, format);

    if (log_output_st != NULL) {
      log_write_output(format, args);
    } else {
      // ignore failure to stdout
      vprintf(format, args);
      putc('\n', stdout);
    }

    va_end(args);
  }
}

void
log_info(const char *format, ...) {
  va_list args;

  // ignore failure to stdout
  va_start(args, format);
  vprintf(format, args);
  putc('\n', stdout);
  va_end(args);

  if (log_output_st != NULL) {
    va_start(args, format);
    log_write_output(format, args);
    va_end(args);
  }
}

void
log_error(const char *format, ...) {
  va_list args;

  // ignore failure to stderr
  va_start(args, format);
  vfprintf(stderr, format, args);
  putc('\n', stderr);
  va_end(args);

  if (log_output_st != NULL) {
    va_start(args, format);
    log_write_output(format, args);
    va_end(args);
  }
}
