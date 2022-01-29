#include <event.h>
#include <event2/event.h>
#include "cont_buf.h"
#include "event_file.h"
#include "event_trigger.h"
#include "file.h"
#include "log.h"
#include "mem.h"

struct event_pipe_file {
  struct event_pipe *pipe;
  struct event_trigger *event;
  int fd;
  size_t max_read_size;
};

static void
event_pipe_file_free(void *arg) {
  if (arg != NULL) {
    struct event_pipe_file *result = (struct event_pipe_file*)arg; // NOLINT

    file_close(event_pipe_get_name(result->pipe), &result->fd);
    event_trigger_release(&result->event);
    free(result);
  }
}

static void
event_on_read(void *arg) {
  assert(arg != NULL);
  struct event_pipe_file *result = (struct event_pipe_file*)arg; // NOLINT
  error_t error_r = event_pipe_read_down(result->pipe);
  if (error_r != 0) {
    log_error(
      "Error when reading file [%s]: %d",
      event_pipe_get_name(result->pipe),
      error_r);
  }
}

static error_t
pipe_on_read(
  struct event_pipe *pipe,
  void *arg,
  struct cont_buf_read *input,
  bool *is_input_end,
  struct cont_buf *output,
  bool *is_buffer_full) {
    assert(arg != NULL);
    assert(input == NULL);
    assert(is_input_end != NULL);
    assert(output != NULL);
    assert(is_buffer_full != NULL);

    struct event_pipe_file *result = (struct event_pipe_file*)arg; // NOLINT
    assert(pipe == result->pipe);

    void *data = NULL;
    size_t available_size;
    cont_buf_write_array_begin(output, 1, &data, &available_size);
    assert(available_size > 0);

    assert(result->max_read_size > 0);
    size_t size = min_size_t(available_size, result->max_read_size);

    assert(result->fd >= 0);
    error_t error_r = file_read(
      event_pipe_get_name(pipe),
      result->fd,
      data, &size);

    if (error_r == 0) {
      if (size > 0) {
        cont_buf_write_array_commit(output, 1, size);
      } else {
        *is_input_end = true;
      }
    }
    return error_r;
  }

static error_t
pipe_on_end(struct event_pipe *pipe, void *arg) {
  assert(arg != NULL);

  struct event_pipe_file *result = (struct event_pipe_file*)arg; // NOLINT
  assert(pipe == result->pipe);

  file_close(
    event_pipe_get_name(pipe),
    &result->fd);
  event_trigger_release(&result->event);

  return 0;
}

static error_t
pipe_on_register_event(struct event_pipe *pipe, void *arg) {
  assert(arg != NULL);

  struct event_pipe_file *result = (struct event_pipe_file*)arg; // NOLINT
  assert(pipe == result->pipe);

  error_t error_r = event_trigger_activate(result->event);
  if (error_r != 0) {
    log_error("Cannot add read event for %s", event_pipe_get_name(pipe));
  }
  return error_r;
}

error_t
event_pipe_from_file(
  struct event_base *loop,
  const struct event_pipe_file_config *config,
  struct event_pipe **result_r) {
    assert(loop != NULL);
    assert(config != NULL);
    assert(config->file_path != NULL);
    assert(result_r != NULL);

    struct event_pipe_file *result = NULL;
    error_t error_r = mem_calloc(
      "event_pipe_file",
      sizeof(struct event_pipe_file),
      (void**)&result); // NOLINT

    if (error_r == 0) {
      EMPTY_STRUCT(event_pipe_config_source, pipe_config);
      pipe_config.arg = result;
      pipe_config.arg_free = event_pipe_file_free;
      pipe_config.on_read = pipe_on_read;
      pipe_config.on_end = pipe_on_end;
      pipe_config.on_register_event = pipe_on_register_event;

      error_r = mem_strdup(config->file_path, &pipe_config.name);
      if (error_r == 0) {
        error_r = cont_buf_create(
          config->buffer_size > 0 ? config->buffer_size : 4096,
          &pipe_config.buffer);
      }
      if (error_r == 0) {
        error_r = event_pipe_create_source(loop, &pipe_config, &result->pipe);
      }
      if (error_r != 0) {
        mem_free((void**)&pipe_config.name); // NOLINT
        cont_buf_release(&pipe_config.buffer);
      }
    }

    if (error_r == 0) {
      error_r = file_open_read(config->file_path, &result->fd);
    }
    if (error_r == 0) {
      result->max_read_size =
        config->max_read_size > 0 ? config->max_read_size : 4096;

      // EPoll doesn't support local files
      error_r = event_trigger_create(
        loop,
        result, event_on_read,
        &result->event);
    }

    if (error_r == 0) {
      *result_r = result->pipe;
    } else if (result != NULL) {
      file_close(config->file_path, &result->fd);
      event_trigger_release(&result->event);
      free(result);
    }
    return error_r;
  }
