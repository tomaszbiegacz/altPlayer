#include <event.h>
#include <event2/event.h>
#include "../log.h"
#include "../mem.h"
#include "../event/event_trigger.h"
#include "../struct/cont_buf.h"
#include "event_file.h"
#include "file.h"

struct event_sink_file {
  struct event_sink *sink;
  struct event_trigger *event;
  int fd;
};

static void
event_sink_file_free(void *arg) {
  if (arg != NULL) {
    struct event_sink_file *result = (struct event_sink_file*)arg; // NOLINT

    file_close(event_sink_get_name(result->sink), &result->fd);
    event_trigger_release(&result->event);
    free(result);
  }
}

static void
event_on_write(void *arg) {
  assert(arg != NULL);
  struct event_sink_file *result = (struct event_sink_file*)arg; // NOLINT
  error_t error_r = event_sink_write(result->sink);
  if (error_r != 0) {
    log_error(
      "Error when writing file [%s]: %d",
      event_sink_get_name(result->sink),
      error_r);
  }
}

static error_t
sink_on_end(struct event_sink *sink, void *arg) {
  assert(arg != NULL);

  struct event_sink_file *result = (struct event_sink_file*)arg; // NOLINT
  assert(sink == result->sink);

  file_close(
    event_sink_get_name(sink),
    &result->fd);
  event_trigger_release(&result->event);

  return 0;
}

static error_t
sink_on_register_event(struct event_sink *sink, void *arg) {
  assert(arg != NULL);

  struct event_sink_file *result = (struct event_sink_file*)arg; // NOLINT
  assert(sink == result->sink);

  error_t error_r = event_trigger_activate(result->event);
  if (error_r != 0) {
    log_error("Cannot add write event for %s", event_sink_get_name(sink));
  }
  return error_r;
}

static error_t
sink_on_write(
  struct event_sink *sink,
  void *arg,
  struct cont_buf_read *input) {
    assert(arg != NULL);
    assert(input != NULL);

    struct event_sink_file *result = (struct event_sink_file*)arg; // NOLINT
    assert(sink == result->sink);

    const void *data = NULL;
    size_t size = cont_buf_read_array_begin(input, 1, &data, 4096);
    assert(size > 0);

    assert(result->fd >= 0);
    error_t error_r = file_write(
      event_sink_get_name(sink),
      result->fd,
      data, &size);

    if (error_r == 0) {
      cont_buf_read_array_commit(input, 1, size);
    }
    return error_r;
  }

error_t
event_sink_into_file(
  struct event_pipe *source,
  const char *file_path,
  struct event_sink **result_r) {
    assert(file_path != NULL);

    struct event_sink_file *result = NULL;
    error_t error_r = mem_calloc(
      "event_sink_file",
      sizeof(struct event_sink_file),
      (void**)&result); // NOLINT

    if (error_r == 0) {
      EMPTY_STRUCT(event_sink_config, sink_config);
      sink_config.arg = result;
      sink_config.arg_free = event_sink_file_free;
      sink_config.on_write = sink_on_write;
      sink_config.on_end = sink_on_end;
      sink_config.on_register_event = sink_on_register_event;

      error_r = mem_strdup(file_path, &sink_config.name);
      if (error_r == 0) {
        error_r = event_sink_create(source, &sink_config, &result->sink);
      }
      if (error_r != 0) {
        mem_free((void**)&sink_config.name); // NOLINT
      }
    }

    if (error_r == 0) {
      error_r = file_open_write(file_path, &result->fd);
    }
    if (error_r == 0) {
      // EPoll doesn't support local files
      error_r = event_trigger_create(
        event_pipe_get_loop(source),
        result, event_on_write,
        &result->event);
    }

    if (error_r == 0) {
      *result_r = result->sink;
    } else if (result != NULL) {
      file_close(file_path, &result->fd);
      event_trigger_release(&result->event);
      free(result);
    }
    return error_r;
  }
