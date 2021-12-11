#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include "log.h"

// session id selected at diagnostics start
static char _log_session_id[16];

// shared diagnostics configuration
bool _log_is_verbose = false;
static FILE *_log_output_st = NULL;

error_t
log_start() {
  unsigned int rand_seed;
  snprintf(_log_session_id, sizeof(_log_session_id), "%x", rand_r(&rand_seed));

  return 0;
}

void
log_free() {
  if (_log_output_st != NULL) {
    error_t result = fclose(_log_output_st);
    if (result != 0) {
      // ignore failure
      fprintf(stderr, "Error when closing log output: %s\n", strerror(result));
    }
    _log_output_st = NULL;
  }
}

void
log_set_verbose(bool value) {
  _log_is_verbose = value;
}

error_t
log_open_output_st(const char *path) {
  assert(path != NULL);
  assert(_log_output_st == NULL);

  _log_output_st = fopen(path, "a");
  if (_log_output_st == NULL) {
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
print_line_beginning(FILE* st) {
  time_t now;
  struct tm local_now;

  // this cannot fail
  time(&now);

  if (localtime_r(&now, &local_now) != NULL) {
    char str_now[32];
    size_t len = strftime(str_now, sizeof(str_now), "%F %H:%M:%S", &local_now);
    if (len > 0) {
      // ignore failure
      fputs("[", st);
      fputs(_log_session_id, st);
      fputs(", ", st);
      fputs(str_now, st);
      fputs("] ", st);
      return;
    }
  }

  // there is some issue with date&time formatting
  // ignore failure
  fputs("[", st);
  fputs(_log_session_id, st);
  fputs("] ", st);
}

static void
print_st(FILE* st, const char *format, va_list args) {
  print_line_beginning(st);
  error_t result = vfprintf(st, format, args);
  if (result < 0) {
    // ignore failure
    fputs("Error when writing logs to output file: ", stderr);
    fputs(strerror(errno), stderr);
    fputs("\n", stderr);
  } else {
    // ignore failure
    putc('\n', st);
  }
}

void
log_verbose(const char *format, ...) {
  if (log_is_verbose()) {
    va_list args;
    va_start(args, format);

    if (_log_output_st != NULL) {
      print_st(_log_output_st, format, args);
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

  if (_log_output_st != NULL) {
    va_start(args, format);
    print_st(_log_output_st, format, args);
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

  if (_log_output_st != NULL) {
    va_start(args, format);
    print_st(_log_output_st, format, args);
    va_end(args);
  }
}

void
log_system_information() {
  if (log_is_verbose()) {
    const unsigned mb = 1024*1024;

    const int page_size = getpagesize();
    const long phys_pages = get_phys_pages();
    const long avphys_pages = get_avphys_pages();

    log_verbose("Page size: %d", page_size);

    log_verbose(
      "Physical memory pages: %d (%d MB)",
      phys_pages,
      phys_pages * page_size / mb);

    log_verbose(
      "Available physical memory pages: %d (%d MB)",
      avphys_pages,
      avphys_pages * page_size / mb);

    double loadavg[3];
    if (getloadavg(loadavg, sizeof(loadavg) / sizeof(double)) != -1) {
      log_verbose("1 minute system load: %f", loadavg[0]);
      log_verbose("5 minute system load: %f", loadavg[1]);
      log_verbose("15 minute system load: %f", loadavg[2]);
    }
  }
}

void
log_full_system_information() {
  if (log_is_verbose()) {
    struct utsname uname_result;
    if (uname(&uname_result) != -1) {
      log_verbose("Machine: %s", uname_result.machine);
      log_verbose("OS name: %s", uname_result.sysname);
      log_verbose(
        "OS release: %s (version: %s)",
        uname_result.release,
        uname_result.version);
      log_verbose("OS host name: %s", uname_result.nodename);
    }

    log_verbose("OS configured procesors: %d", get_nprocs_conf());
    log_verbose("Available procesors: %d", get_nprocs());
  }
  log_system_information();
}
