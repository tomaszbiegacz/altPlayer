#include "event_sink.h"
#include "mem.h"

struct event_sink {
  struct event_pipe *pipe;

  void *arg;
  free_f arg_free;

  struct event_pipe *source;
  struct cont_buf_read *input;
  bool is_end;

  event_sink_on_write *on_write;
  event_sink_on_event *on_register_event;
  event_sink_on_event *on_end;
  event_sink_on_error *on_error;
};

static void
sink_free(void *arg) {
  assert(arg != NULL);
  struct event_sink *sink = (struct event_sink*)arg;
  if (sink->arg_free != NULL) {
    sink->arg_free(sink->arg);
  }
  free(sink);
}

static error_t
sink_register_event(struct event_sink *sink) {
  error_t error_r = 0;
  if (!cont_buf_read_is_empty(sink->input)) {
    error_r = sink->on_register_event(sink, sink->arg);
  }
  return error_r;
}

static error_t
pipe_on_read(
  struct event_pipe *pipe,
  void *arg,
  struct cont_buf_read *input,
  bool *is_input_end,
  struct cont_buf *output) {
    assert(arg != NULL);
    assert(output == NULL);
    UNUSED(is_input_end);

    struct event_sink *sink = (struct event_sink*)arg;
    assert(sink->pipe == pipe);

    if (sink->input == NULL) {
      sink->input = input;
    } else {
      assert(input == sink->input);
    }
    return sink_register_event(sink);
  }

static bool
event_sink_is_source_drained(const struct event_sink *sink) {
  bool is_source_end = event_pipe_is_end(sink->source);
  bool is_source_empty = cont_buf_read_is_empty(sink->input);
  return is_source_end && is_source_empty;
}

static error_t
event_sink_end(struct event_sink *sink) {
  error_t error_r = 0;
  if (!sink->is_end && sink->on_end != NULL)  {
    error_r = sink->on_end(sink, sink->arg);
  }
  if (error_r == 0) {
    sink->is_end = true;
  }
  return error_r;
}

static error_t
pipe_on_end(struct event_pipe *pipe, void *arg) {
  assert(arg != NULL);

  struct event_sink *sink = (struct event_sink*)arg;
  assert(sink->pipe == pipe);

  error_t error_r = 0;
  if (event_sink_is_source_drained(sink)) {
    error_r = event_sink_end(sink);
  }
  return error_r;
}

static void
pipe_on_error(struct event_pipe *pipe, void *arg) {
  assert(arg != NULL);

  struct event_sink *sink = (struct event_sink*)arg;
  assert(sink->pipe == pipe);

  sink->on_error(sink, sink->arg);
}

static void
default_on_error(struct event_sink *sink, void *arg) {
  assert(sink != NULL);
  if (sink->on_end != NULL) {
    sink->on_end(sink, arg);
  }
}

error_t
event_sink_create(
  struct event_pipe *source,
  struct event_sink_config *config,
  struct event_sink **result_r) {
    assert(source != NULL);
    assert(config != NULL);
    assert(config->name != NULL);
    if (config->arg_free != NULL) {
      assert(config->arg != NULL);
    }
    assert(config->on_write != NULL);
    assert(config->on_register_event != NULL);
    assert(result_r != NULL);

    struct event_sink *result = NULL;
    error_t error_r = mem_calloc(
      "event_sink",
      sizeof(struct event_sink),
      (void**)&result); // NOLINT

    if (error_r == 0) {
      result->source = source;
      result->arg = config->arg;
      result->arg_free = config->arg_free;
      result->on_write = config->on_write;
      result->on_register_event = config->on_register_event;
      result->on_end = config->on_end;
      result->on_error = IF_NULL(config->on_error, default_on_error);

      EMPTY_STRUCT(event_pipe_config, pipe_config);
      pipe_config.name = config->name;
      pipe_config.arg = result;
      pipe_config.arg_free = sink_free;
      pipe_config.on_read = pipe_on_read;
      pipe_config.on_end = pipe_on_end;
      pipe_config.on_error = pipe_on_error;
      error_r = event_pipe_create(source, &pipe_config, &result->pipe);
    }

    if (error_r == 0) {
      *result_r = result;
    } else {
      mem_free((void**)&result);  // NOLINT
    }
    return error_r;
  }

const char*
event_sink_get_name(const struct event_sink *sink) {
  assert(sink != NULL);
  return event_pipe_get_name(sink->pipe);
}

bool
event_sink_is_end(const struct event_sink *sink) {
  assert(sink != NULL);
  return sink->is_end;
}

static error_t
event_sink_write_into(struct event_sink *sink) {
  assert(!cont_buf_read_is_empty(sink->input));
  error_t error_r = sink->on_write(sink, sink->arg, sink->input);
  if (error_r == 0 && !event_pipe_is_end(sink->pipe)) {
    error_r = event_pipe_read_up(sink->pipe);
  }
  if (error_r == 0) {
    if (event_sink_is_source_drained(sink)) {
      error_r = event_sink_end(sink);
    } else {
      error_r = sink_register_event(sink);
    }
  }
  if (error_r != 0) {
    event_pipe_error_from_source(sink->pipe);
  }
  return error_r;
}

error_t
event_sink_write(struct event_sink *sink) {
  assert(sink != NULL);

  if (sink->input == NULL) {
    // sink is not ready yet to write anything
    return 0;
  } else {
    return event_sink_write_into(sink);
  }
}
