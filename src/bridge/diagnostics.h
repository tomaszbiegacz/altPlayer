#ifndef _H_DIAGNOSTICS
#define _H_DIAGNOSTICS

/**
 * always print to output
 */
void
log_info(const char *format, ...);

/**
 * log output if verbose diagnostics is enabled
 */
void
log_verbose(const char *format, ...);

#endif