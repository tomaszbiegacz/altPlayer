#pragma once
#include "../structs/cont_buf.h"
#include "event_loop.h"

/**
 * @brief Event pipe reads. Possible event pipes:
 * - source: this will read data from external source like file or socket.
 * - intermediate: this reuses output buffer from previous pipe. It is used
 *    mostly for parsing stream's header and once done passing remaining
 *    stream to next pipes.
 * - buffered: translates input stream into the output stream by processing
 *    it.
 */
struct event_pipe;

/**
 * @brief Read trigger
 *
 * @param pipe Event pipe handle
 * @param arg Custom state for pipe processing
 * @param input Input buffer, for source pipe this is NULL
 * @param is_input_end Return information whether input stream has completed.
 *  For intermediate pipe set to true to signal end of it's processing
 *  and allow read on for the next pipe.
 *  For buffered pipes this is NULL.
 * @param output Output buffer. For intermediate pipe this is NULL
 * @param is_buffer_full For output buffered pipe return information whether
 *  all available space has been used.
 */
typedef error_t event_pipe_on_read (
  struct event_pipe *pipe,
  void *arg,
  struct cont_buf_read *input,
  bool *is_input_end,
  struct cont_buf *output);

typedef error_t event_pipe_on_event (
  struct event_pipe *pipe,
  void *arg);

typedef void event_pipe_on_error (
  struct event_pipe *pipe,
  void *arg);

//
// Setup
//

/**
 * @brief Event pipe configuraiton for source pipe
 *
 * @param name Mandatory event pipe name
 * @param arg Optional custom state used when processing pipe events
 * @param arg_free Optional function responsible for releasing custom state
 * @param buffer Mandatory output buffer for pipe processing
 * @param on_read Mandatory trigger used for reading into output buffer.
 * @param on_register_event Mandatory callback executed
 *  when pipe can accept another read after "on_read" call.
 * @param on_end Optional callback to release resources after
 *  pipe completes reading
 * @param on_error Optional callback for processing events ends up
 *  with an error. If not given "on_end" will be used (if given).
 */
struct event_pipe_config_source {
  char* name;

  void *arg;
  free_f arg_free;

  struct cont_buf *buffer;

  event_pipe_on_read *on_read;
  event_pipe_on_event *on_register_event;
  event_pipe_on_event *on_end;
  event_pipe_on_error *on_error;
};

/**
 * @brief Event pipe configuraiton
 *
 * @param name Mandatory event pipe name
 * @param arg Optional custom state used when processing pipe events
 * @param arg_free Optional function responsible for releasing custom state
 * @param buffer Optional output buffer for pipe processing
 * @param on_read Mandatory callback for transforming input into output
 *  or parsing input if "buffer" is NULL.
 *  Reading callback should try to do as much progress as possible in one
 *  call. This callback is not exepected to be called in a loop.
 * @param on_end Optional callback to release resources after
 *  pipe completes reading
 * @param on_error Optional callback for processing events ends up
 *  with an error. If not given "on_end" will be used (if given).
 */
struct event_pipe_config {
  char* name;

  void *arg;
  free_f arg_free;

  struct cont_buf *buffer;

  event_pipe_on_read *on_read;
  event_pipe_on_event *on_end;
  event_pipe_on_error *on_error;
};

/**
 * @brief Create source event pipe
 */
error_t
event_pipe_create_source(
  struct event_base *loop,
  struct event_pipe_config_source *config,
  struct event_pipe **result_r);

/**
 * @brief Create intermediate event pipe
 */
error_t
event_pipe_create(
  struct event_pipe *source,
  struct event_pipe_config *config,
  struct event_pipe **result_r);

/**
 * @brief Free resource of the event pipe with all subsequent pipes.
 * This should be called only on pipe's source.
 */
void
event_pipe_release(struct event_pipe **result_r);

//
// Query
//

/**
 * @brief Get pipe's event loop
 */
struct event_base*
event_pipe_get_loop(struct event_pipe *pipe);

/**
 * @brief Get pipe's name
 */
const char*
event_pipe_get_name(const struct event_pipe *pipe);

/**
 * @brief "on_read" is being called only if there is
 * at minimum "lowmark" data in the input buffer.
 * For source pipe "lowmark" is ignored.
 */
size_t
event_pipe_get_read_lowmark(const struct event_pipe *pipe);

/**
 * @brief For not intermediate pipes "on_read" is being called
 * only if there is at minimum "lowmark" available space in the input buffer.
 * For intermediate pipes "lowmark" is ignored.
 */
size_t
event_pipe_get_write_lowmark(const struct event_pipe *pipe);

/**
 * @brief No input can be read into the output buffer.
 * For intermediate pipe this is inherited from the previous pipe.
 */
bool
event_pipe_is_end(const struct event_pipe *pipe);

/**
 * @brief For buffered pipe return positive if pipe is marked as "end"
 * or there is not enough space in the buffer left for writing (see "lowmark").
 * For intermediate pipe this will be positive once
 * it's processing will complete.
 */
bool
event_pipe_is_buffer_full(const struct event_pipe *pipe);

/**
 * @brief For intermediate pipe returns true.
 * For buffered or source pipe return true if no data is available in input.
 */
bool
event_pipe_is_empty(const struct event_pipe *pipe);

/**
 * @brief How many times read callback has been called?
 */
size_t
event_pipe_get_read_count(const struct event_pipe *pipe);

//
// Command
//

/**
 * @brief Sets read lowmark, see "event_pipe_get_read_lowmark"
 */
void
event_pipe_set_read_lowmark(
  struct event_pipe *pipe,
  size_t lowmark);

/**
 * @brief Sets write lowmark, see "event_pipe_get_write_lowmark".
 * For intermediate pipe, this is efectively setting "lowmark" at
 * the buffered pipe level.
 */
void
event_pipe_set_write_lowmark(
  struct event_pipe *pipe,
  size_t lowmark);

/**
 * @brief Make sure that output buffer is exactly at given size.
 * This will fail badly for intermediate pipes.
 */
error_t
event_pipe_buffer_size(
  struct event_pipe *pipe,
  size_t min_buffer_size);

/**
 * @brief Trigger "on_read" if there is a space in the output buffer.
 * Continue reading down the pipe.
 * If input stream is closed, EIO will be returned.
 */
error_t
event_pipe_read_down(struct event_pipe *pipe);

/**
 * @brief Trigger "on_read" on current pipe if it is ready for read.
 * Continue reading up the pipe.
 * This will not have effect on source pipe.
 * If input stream is closed, EIO will be returned.
 */
error_t
event_pipe_read_up(struct event_pipe *pipe);

/**
 * @brief Find root and report error for each pipe downward
 */
void
event_pipe_error_from_source(struct event_pipe *pipe);
