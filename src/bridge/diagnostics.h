#ifndef _H_DIAGNOSTICS
#define _H_DIAGNOSTICS

/**
 * always print to stdout
 */
void
log_info(const char *format, ...);

/**
 * log output if verbose diagnostics is enabled
 */
void
log_verbose(const char *format, ...);

/**
 * always print to stderr
 */
void
log_error(const char *format, ...);

#endif