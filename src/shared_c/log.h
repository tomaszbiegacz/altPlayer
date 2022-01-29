#pragma once
#include "shrdef.h"

extern bool _log_is_verbose;


//
// Setup
//

/**
 * Enable or disable printing out verbose logs.
 */
void
log_set_verbose(bool value);

/**
 * Append logs to additional output file stream.
 */
error_t
log_append_to_file(const char *path);

/**
 * Free resources allocated by diagnostics module.
 * Should be called when program ends.
 */
void
log_global_release();


//
// Query
//

/**
 * is verbose diagnostics enabled?
 */
static inline bool
log_is_verbose() {
  return _log_is_verbose;
}


//
// Command
//

/**
 * always prints to stdout
 */
void
log_info(const char *format, ...);

/**
 * log if verbose diagnostics is enabled
 */
void
log_verbose(const char *format, ...);

/**
 * always prints to stderr
 */
void
log_error(const char *format, ...);

/**
 * if verbose diagnostics is enabled
 * write extended system information like OS name, avaiable memory etc
 */
void
log_system_information();

/**
 * if verbose diagnostics is enabled
 * write basic system information like avaiable memory etc
 */
void
log_system_load_information();
