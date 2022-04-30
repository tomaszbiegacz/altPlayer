#include "../mem.h"
#include "../log.h"
#include "../struct/cont_buf.h"
#include "event_pipe.h"

/**
 * @brief Event pipe defition
 *
 * @param loop Owning loop
 * @param name Pipe's name, stored for diagnostics purposes
 * @param arg Custom pipe state
 * @param arg_free Optional function for releasing custom state
 * @param buffer Optional buffer for pipe processing
 * @param is_buffer_full Pipe's owner decides when pipe is full
 * @param read_lowmark Until there not enough bytes in pipe, don't call cbs
 * @param on_read Called on reading input
 * @param on_register_event Called when pipe is ready to digest more input
 * @param on_end Called when pipe already knows all input that is left
 * @param on_error Called on error as part of pipe's processing
 * @param prev Previous pipe in the chain
 * @param next Next pipe in the chain
 */
struct event_pipe {
  struct event_base *loop;
  char *name;

  void *arg;
  free_f arg_free;

  // ignored for intermediate pipe
  struct cont_buf *buffer;
  size_t write_lowmark;

  size_t read_lowmark;
  bool is_end;
  bool is_next_enabled;
  size_t read_count;

  event_pipe_on_read *on_read;
  event_pipe_on_event *on_register_event;
  event_pipe_on_event *on_end;
  event_pipe_on_error *on_error;

  struct event_pipe *prev;
  struct event_pipe *next;
};

static void
default_on_error(struct event_pipe *pipe, void *arg) {
  assert(pipe != NULL);
  if (pipe->on_end != NULL) {
    pipe->on_end(pipe, arg);
  }
}

error_t
event_pipe_create_source(
  struct event_base *loop,
  struct event_pipe_config_source *config,
  struct event_pipe **result_r) {
    assert(loop != NULL);
    assert(config != NULL);
    assert(config->name != NULL);
    if (config->arg_free != NULL) {
      assert(config->arg != NULL);
    }
    assert(config->buffer != NULL);
    assert(config->on_read != NULL);
    assert(config->on_register_event != NULL);
    assert(result_r != NULL);

    struct event_pipe *result = NULL;
    error_t error_r = mem_calloc(
      "event_pipe",
      sizeof(struct event_pipe),
      (void**)&result); // NOLINT

    if (error_r == 0) {
      result->loop = loop;
      result->name = config->name;
      result->arg = config->arg;
      result->arg_free = config->arg_free;

      result->buffer = config->buffer;
      result->write_lowmark = 1;

      result->read_lowmark = 1;
      result->is_end = false;
      result->is_next_enabled = true;
      result->read_count = 0;

      result->on_read = config->on_read;
      result->on_end = config->on_end;
      result->on_register_event = config->on_register_event;
      result->on_error = IF_NULL(config->on_error, default_on_error);

      *result_r = result;
    }
    return error_r;
  }

error_t
event_pipe_create(
  struct event_pipe *source,
  struct event_pipe_config *config,
  struct event_pipe **result_r) {
    assert(source != NULL);
    assert(source->next == NULL);
    assert(config != NULL);
    assert(config->name != NULL);
    if (config->arg_free != NULL) {
      assert(config->arg != NULL);
    }
    assert(config->on_read != NULL);
    assert(result_r != NULL);

    struct event_pipe *result = NULL;
    error_t error_r = mem_calloc(
      "event_pipe",
      sizeof(struct event_pipe),
      (void**)&result); // NOLINT

    if (error_r == 0) {
      result->loop = event_pipe_get_loop(source);
      result->name = config->name;
      result->arg = config->arg;
      result->arg_free = config->arg_free;

      result->buffer = config->buffer;
      result->write_lowmark = 1;

      result->read_lowmark = 1;
      result->is_end = false;
      result->is_next_enabled = config->buffer != NULL;
      result->read_count = 0;

      result->on_read = config->on_read;
      result->on_end = config->on_end;
      result->on_register_event = NULL;
      result->on_error = IF_NULL(config->on_error, default_on_error);

      result->prev = source;
      source->next = result;

      *result_r = result;
    }
    return error_r;
  }

void
event_pipe_release(struct event_pipe **result_r) {
  assert(result_r != NULL);

  struct event_pipe *result = *result_r;
  if (result != NULL) {
    log_verbose("Releasing event [%s]", result->name);
    if (result->arg_free != NULL) {
      result->arg_free(result->arg);
    }
    mem_free((void**)&result->name);  // NOLINT
    cont_buf_release(&result->buffer);

    if (result->next != NULL) {
      event_pipe_release(&result->next);
    }
    if (result->prev != NULL) {
      result->prev->next = NULL;
    }

    free(result);
    *result_r = NULL;
  }
}

//
// Utilities
//


static inline bool
is_source(const struct event_pipe *pipe) {
  return pipe->prev == NULL;
}

static inline bool
is_intermediate(const struct event_pipe *pipe) {
  return pipe->buffer == NULL;
}

static inline bool
is_intermediate_processed(const struct event_pipe *pipe) {
  return is_intermediate(pipe) && pipe->is_next_enabled;
}

inline static bool
is_sink(const struct event_pipe *pipe) {
  return pipe->next == NULL;
}

static struct event_pipe*
get_source(struct event_pipe *pipe) {
  while (!is_source(pipe)) {
    pipe = pipe->prev;
  }
  return pipe;
}

static struct event_pipe*
get_input_pipe(struct event_pipe *pipe) {
  assert(pipe != NULL);
  assert(!is_source(pipe));

  do {
    pipe = pipe->prev;
  } while (pipe->buffer == NULL);
  return pipe;
}

inline static struct cont_buf_read*
get_input(struct event_pipe *pipe) {
  struct event_pipe *input_pipe = get_input_pipe(pipe);
  return cont_buf_read(input_pipe->buffer);
}

//
// Queries
//

struct event_base*
event_pipe_get_loop(struct event_pipe *pipe) {
  assert(pipe != NULL);
  return pipe->loop;
}

const char*
event_pipe_get_name(const struct event_pipe *pipe) {
  assert(pipe != NULL);
  return pipe->name;
}

size_t
event_pipe_get_read_lowmark(const struct event_pipe *pipe) {
  assert(pipe != NULL);
  return pipe->read_lowmark;
}

size_t
event_pipe_get_write_lowmark(const struct event_pipe *pipe) {
  assert(pipe != NULL);
  return pipe->write_lowmark;
}

error_t
event_pipe_set_read_lowmark(
  struct event_pipe *pipe,
  size_t lowmark) {
    assert(pipe != NULL);
    assert(lowmark > 0);
    error_t error_r = 0;
    struct event_pipe *input = get_input_pipe(pipe);
    if (lowmark >= cont_buf_get_available_size(input->buffer)) {
      error_r = event_pipe_buffer_size(input, 2 * lowmark);
    }
    if (error_r == 0) {
      pipe->read_lowmark = lowmark;
    }
    return error_r;
  }

error_t
event_pipe_set_write_lowmark(
  struct event_pipe *pipe,
  size_t lowmark) {
    assert(pipe != NULL);
    assert(pipe->buffer != NULL);
    assert(lowmark > 0);
    error_t error_r = 0;
    if (lowmark >= cont_buf_get_available_size(pipe->buffer)) {
      error_r = event_pipe_buffer_size(pipe, 2 * lowmark);
    }
    if (error_r == 0) {
      pipe->write_lowmark = lowmark;
    }
    return error_r;
  }

error_t
event_pipe_buffer_size(
  struct event_pipe *pipe,
  size_t buffer_size) {
    assert(pipe != NULL);
    assert(pipe->buffer != NULL);
    return cont_buf_resize(pipe->buffer, buffer_size);
  }

bool
event_pipe_is_end(const struct event_pipe *pipe) {
  assert(pipe != NULL);
  return pipe->is_end;
}

static bool
event_pipe_is_end_effective(struct event_pipe *pipe) {
  if (is_source(pipe)) {
    return pipe->is_end;
  }

  bool is_prev_end = event_pipe_is_end(pipe->prev);
  if (is_intermediate(pipe) && !is_sink(pipe)) {
    return is_prev_end && is_intermediate_processed(pipe);
  } else {
    struct cont_buf_read *input = get_input(pipe);
    return is_prev_end && cont_buf_read_is_empty(input);
  }
}

bool
event_pipe_is_buffer_full(const struct event_pipe *pipe) {
  if (is_intermediate(pipe)) {
    return is_intermediate_processed(pipe);
  } else {
    size_t available_size = cont_buf_get_available_size(pipe->buffer);
    return pipe->is_end || available_size <= pipe->write_lowmark;
  }
}

bool
event_pipe_is_empty(const struct event_pipe *pipe) {
  if (is_intermediate(pipe)) {
    return true;
  } else {
    return cont_buf_is_empty(pipe->buffer);
  }
}

size_t
event_pipe_get_read_count(const struct event_pipe *pipe) {
  assert(pipe != NULL);
  return pipe->read_count;
}

//
// Commands
//

error_t
event_pipe_buffer_resize(
  struct event_pipe *pipe,
  size_t buffer_size) {
    assert(pipe != NULL);
    assert(!is_intermediate(pipe));
    return cont_buf_resize(pipe->buffer, buffer_size);
  }

static void
event_pipe_error_from(struct event_pipe *pipe) {
  if (pipe->on_error != NULL) {
    pipe->on_error(pipe, pipe->arg);
  }
  pipe->is_end = true;
  if (pipe->next != NULL) {
    event_pipe_error_from(pipe->next);
  }
}

void
event_pipe_error_from_source(struct event_pipe *pipe) {
  assert(pipe != NULL);
  struct event_pipe *root = get_source(pipe);
  event_pipe_error_from(root);
}

/**
 * @brief Is given pipe ready for read operation?
 */
static bool
event_pipe_is_read_ready(struct event_pipe *pipe) {
  if (is_source(pipe)) {
    // is there anything to read from the source
    return !pipe->is_end;
  } else if (pipe->is_end || is_intermediate_processed(pipe)) {
    // pipe processing is completed
    return false;
  } else {
    struct cont_buf_read *read = get_input(pipe);
    size_t unread = cont_buf_get_unread_size(read);
    if (event_pipe_is_end(pipe->prev)) {
      // finish it
      return true;
    } else {
      return unread >= pipe->read_lowmark;
    }
  }
}

static void
apply_no_padding_strategy(struct event_pipe *pipe) {
  struct cont_buf *buffer = pipe->buffer;
  size_t allocated = cont_buf_get_allocated_size(buffer);
  size_t unread = cont_buf_get_unread_size(cont_buf_read(buffer));
  if (unread < allocated / 2) {
    cont_buf_no_padding(buffer);
  }
}

static error_t
trigger_on_read(
  struct event_pipe *pipe,
  bool *is_input_end) {
    assert(!is_intermediate_processed(pipe));
    bool is_buffered = false;
    bool is_buffered_end = false;

    bool *is_end = NULL;
    if (is_source(pipe)) {
      is_end = is_input_end;
    } else if (is_intermediate(pipe)) {
      is_end = &pipe->is_next_enabled;
    } else {
      is_buffered = true;
      is_end = &is_buffered_end;
    }

    error_t error_r = pipe->on_read(
      pipe, pipe->arg,
      is_source(pipe) ? NULL : get_input(pipe),
      is_end,
      pipe->buffer);

    if (error_r == 0) {
      pipe->read_count++;

      if (is_buffered) {
        // maybe pipe has processed all input
        bool is_prev_end = event_pipe_is_end(pipe->prev);
        *is_input_end = is_prev_end && is_buffered_end;
      }
    }

    return error_r;
  }

static error_t
issue_register_event(struct event_pipe *pipe) {
  assert(!pipe->is_end);
  assert(pipe->on_register_event != NULL);
  error_t error_r = 0;
  apply_no_padding_strategy(pipe);
  if (!event_pipe_is_buffer_full(pipe)) {
    error_r = pipe->on_register_event(pipe, pipe->arg);
  }
  return error_r;
}

static error_t
callbacks_after_read(struct event_pipe *pipe, bool is_input_end) {
  error_t error_r = 0;
  if (is_input_end && !pipe->is_end) {
      if (pipe->on_end != NULL) {
        error_r = pipe->on_end(pipe, pipe->arg);
      }
      if (error_r == 0) {
        pipe->is_end = true;
      }
  }
  if (error_r == 0 && pipe->next != NULL && pipe->is_next_enabled) {
    if (!event_pipe_is_end(pipe->next)) {
      error_r = event_pipe_read_down(pipe->next);
    }
  }

  if (error_r == 0 && is_source(pipe) && !pipe->is_end) {
    if (is_source(pipe)) {
      error_r = issue_register_event(pipe);
    } else {
      apply_no_padding_strategy(pipe);
    }
  }
  return error_r;
}

static bool
event_pipe_is_drained(struct event_pipe *pipe) {
  if (is_source(pipe)) {
    // this can only be decided as part of "on_read" callback
    return pipe->is_end;
  } else {
    bool is_prev_end = event_pipe_is_end(pipe->prev);
    bool is_input_empty = cont_buf_read_is_empty(get_input(pipe));
    return is_prev_end && is_input_empty;
  }
}

error_t
event_pipe_read_down(struct event_pipe *pipe) {
  if (event_pipe_is_end(pipe)) {
    log_error(
      "Pipe [%s] is already completed, read is not possible",
      pipe->name);
    return EIO;
  }

  error_t error_r = 0;
  bool is_input_ready = event_pipe_is_read_ready(pipe);
  bool is_ouput_ready = !event_pipe_is_buffer_full(pipe);
  if (is_input_ready && is_ouput_ready) {
    bool is_end = event_pipe_is_end_effective(pipe);
    if (!(is_end && is_intermediate(pipe))) {
      error_r = trigger_on_read(pipe, &is_end);
    }
    if (error_r == 0) {
      error_r = callbacks_after_read(pipe, is_end);
    }
  } else if (is_intermediate_processed(pipe)) {
    bool is_end = event_pipe_is_end_effective(pipe);
    error_r = callbacks_after_read(pipe, is_end);
  } else if (event_pipe_is_drained(pipe)) {
    error_r = callbacks_after_read(pipe, true);
  }

  if (error_r != 0) {
    event_pipe_error_from_source(pipe);
  }
  return error_r;
}

static error_t
read_up_the_pipe(struct event_pipe *pipe) {
  error_t error_r = 0;
  // intermediate pipes can be skipped
  struct event_pipe *input_owner = get_input_pipe(pipe);
  assert(input_owner != pipe);

  if (!event_pipe_is_end(input_owner)) {
    error_r = event_pipe_read_up(input_owner);
  }
  return error_r;
}

error_t
event_pipe_read_up(struct event_pipe *pipe) {
  if (event_pipe_is_end(pipe)) {
    log_error(
      "Pipe [%s] is already completed, unblocking is not possible",
      pipe->name);
    return EIO;
  }

  error_t error_r = 0;
  if (is_source(pipe)) {
    error_r = issue_register_event(pipe);
  } else if (is_sink(pipe)) {
    // reading at sink level will not really help, move up
    error_r = read_up_the_pipe(pipe);
  } else if(!event_pipe_is_end(pipe)) {
    apply_no_padding_strategy(pipe);
    while (
      error_r == 0
      && !event_pipe_is_end(pipe)
      && !event_pipe_is_buffer_full(pipe)
      ) {
        if (event_pipe_is_read_ready(pipe)) {
          // fill-up the pipe
          bool is_end = event_pipe_is_end_effective(pipe);
          if (!(is_end && is_intermediate(pipe))) {
            error_r = trigger_on_read(pipe, &is_end);
          }
          if (error_r == 0) {
            error_r = callbacks_after_read(pipe, is_end);
          }
          if (error_r == 0) {
            error_r = read_up_the_pipe(pipe);
          }
        } else {
          // try to fill-up the buffer
          error_r = read_up_the_pipe(pipe);
          if (error_r == 0 && !event_pipe_is_read_ready(pipe)) {
            // this hasn't helped, let's finish here
            return 0;
          }
        }
      }
    if (error_r != 0) {
      event_pipe_error_from_source(pipe);
    }
  }

  return error_r;
}


