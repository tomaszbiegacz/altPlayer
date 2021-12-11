#ifndef PLAYER_LOG_H_
#define PLAYER_LOG_H_

#include <assert.h>
#include "shrdef.h"

extern bool _log_is_verbose;

/**
 * Initializes diagnostics.
 */
error_t
log_start();

/**
 * Free resources allocated by diagnostics module.
 * Should be called when program ends.
 */
void
log_free();

/**
 * is verbose diagnostics enabled?
 */
static inline bool
log_is_verbose() {
  return _log_is_verbose;
}

/**
 * enable or disable printing out verbose logs
 */
void
log_set_verbose(bool value);

/**
 * append logs to additional output file stream
 */
error_t
log_open_output_st(const char *path);

/**
 * always print to stdout
 */
void
log_info(const char *format, ...);

/**
 * log if verbose diagnostics is enabled
 */
void
log_verbose(const char *format, ...);

/**
 * always print to stderr
 */
void
log_error(const char *format, ...);

/**
 * if verbose diagnostics is enabled
 * write extended system information like OS name, avaiable memory etc
 */
void
log_full_system_information();

/**
 * if verbose diagnostics is enabled
 * write basic system information like avaiable memory etc
 */
void
log_system_information();

#endif
