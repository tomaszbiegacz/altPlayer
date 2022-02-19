#pragma once
#include "event_pipe.h"

struct event_sink;

typedef error_t event_sink_on_write (
  struct event_sink *sink,
  void *arg,
  struct cont_buf_read *input);

typedef error_t event_sink_on_event (
  struct event_sink *sink,
  void *arg);

typedef void event_sink_on_error (
  struct event_sink *sink,
  void *arg);

//
// Setup
//

/**
 * @brief Event sink configuration
 *
 * @param name Mandatory event sink name
 * @param arg Optional custom state used when processing sink events
 * @param arg_free Optional function responsible for releasing custom state
 * @param on_write Mandatory for writing input into some medium
 * @param on_register_event Mandatory callback executed
 *  when pipe is ready for write after "on_write" call.
 * @param on_end Optional callback to release resources after
 *  pipe completes writing
 * @param on_error Optional callback for processing events ends up
 *  with an error. If not given "on_end" will be used (if given).
 */
struct event_sink_config {
  char* name;

  void *arg;
  free_f arg_free;

  event_sink_on_write *on_write;
  event_sink_on_event *on_register_event;
  event_sink_on_event *on_end;
  event_sink_on_error *on_error;
};

/**
 * @brief Create event sink.
 */
error_t
event_sink_create(
  struct event_pipe *source,
  struct event_sink_config *config,
  struct event_sink **result_r);

//
// Query
//

const char*
event_sink_get_name(const struct event_sink *sink);

bool
event_sink_is_end(const struct event_sink *sink);

//
// Command
//

/**
 * @brief Trigger pipe write operation if there is anything to read in
 * the source pipe.
 * If output stream is already closed, EIO will be returned.
 */
error_t
event_sink_write(struct event_sink *sink);
