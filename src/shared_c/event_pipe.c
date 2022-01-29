#include "cont_buf.h"
#include "event_pipe.h"
#include "mem.h"
#include "log.h"

struct event_pipe {
  struct event_base *loop;
  char* name;

  void *arg;
  free_f arg_free;

  // ignored for intermediate pipe
  struct cont_buf *buffer;
  bool is_buffer_full;

  size_t read_lowmark;
  bool is_end;
  bool is_next_enabled;

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
      result->is_buffer_full = false;

      result->read_lowmark = 1;
      result->is_end = false;
      result->is_next_enabled = true;

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
      result->is_buffer_full = false;

      result->read_lowmark = 1;
      result->is_end = false;
      result->is_next_enabled = config->buffer != NULL;

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
    log_verbose("Closing event %s", result->name);
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

void
event_pipe_set_lowmark(
  struct event_pipe *pipe,
  size_t lowmark) {
    assert(pipe != NULL);
    assert(lowmark > 0);
    pipe->read_lowmark = lowmark;
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
  if (is_intermediate(pipe)) {
    return is_prev_end;
  } else {
    struct cont_buf_read *input = get_input(pipe);
    return is_prev_end && cont_buf_read_is_empty(input);
  }
}

bool
event_pipe_is_buffer_full(const struct event_pipe *pipe) {
  if (is_intermediate(pipe)) {
    if (is_intermediate_processed(pipe)) {
      return true;
    } else {
      pipe = get_input_pipe((struct event_pipe*)pipe); // NOLINT
    }
  }
  return pipe->is_end || pipe->is_buffer_full;
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
    return unread >= pipe->read_lowmark;
  }
}

static void
apply_no_padding_strategy(struct event_pipe *pipe) {
  struct cont_buf *buffer = pipe->buffer;
  size_t allocated = cont_buf_get_allocated_size(buffer);
  size_t unread = cont_buf_get_unread_size(cont_buf_read(buffer));
  if (unread < allocated / 2) {
    cont_buf_no_padding(buffer);
    pipe->is_buffer_full = false;
  }
}

static error_t
trigger_on_read(
  struct event_pipe *pipe,
  bool *is_input_end) {
    assert(!is_intermediate_processed(pipe));
    struct cont_buf_read *input = is_source(pipe) ? NULL : get_input(pipe);

    bool is_full = false;
    error_t error_r = pipe->on_read(
      pipe, pipe->arg,
      input,
      input == NULL ? is_input_end : NULL,
      pipe->buffer,
      &is_full);

    if (error_r == 0) {
      if (pipe->buffer != NULL) {
        if (!is_full) {
          // extra safe
          is_full = cont_buf_is_full(pipe->buffer);
        }
        pipe->is_buffer_full = is_full;
      } else {
        pipe->is_next_enabled = is_full;
      }
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
    assert(!event_pipe_is_end(pipe->next));
    error_r = event_pipe_read_down(pipe->next);
  }

  if (error_r == 0 && !is_input_end) {
    if (!is_intermediate(pipe)) {
      apply_no_padding_strategy(pipe);
    }
    if (pipe->on_register_event != NULL) {
      error_r = pipe->on_register_event(pipe, pipe->arg);
    }
  }
  return error_r;
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
  bool is_end = event_pipe_is_end_effective(pipe);
  bool is_input_ready = event_pipe_is_read_ready(pipe);
  bool is_ouput_ready = !event_pipe_is_buffer_full(pipe);
  if (is_input_ready && is_ouput_ready) {
    assert(!is_end);
    error_r = trigger_on_read(pipe, &is_end);
  }
  if (error_r == 0) {
    error_r = callbacks_after_read(pipe, is_end);
  }
  if (error_r != 0) {
    event_pipe_error_from_source(pipe);
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
  if (is_source(pipe)) {
    return 0;
  }

  error_t error_r = 0;
  while (
    error_r == 0
    && !event_pipe_is_end(pipe)
    && event_pipe_is_read_ready(pipe)
    && !event_pipe_is_buffer_full(pipe)) {
      bool is_end = event_pipe_is_end_effective(pipe);
      error_r = trigger_on_read(pipe, &is_end);
      if (error_r == 0) {
        error_r = callbacks_after_read(pipe, is_end);
      }
      if (error_r == 0) {
        // this is mean to fill-up the buffers
        // hence intermediate pipes can be skipped
        struct event_pipe *input_owner = get_input_pipe(pipe);
        if (!event_pipe_is_end(input_owner)) {
          error_r = event_pipe_read_up(input_owner);
        }
      } else {
        event_pipe_error_from_source(pipe);
      }
    }
  return error_r;
}


