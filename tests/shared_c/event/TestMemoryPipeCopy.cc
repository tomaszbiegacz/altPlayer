#include "TestMemoryPipeCopy.h"

extern "C" {
  #include "mem.h"
}

static error_t
on_read(
  struct event_pipe *pipe,
  void *arg,
  struct cont_buf_read *input,
  bool *is_input_end,
  struct cont_buf *output) {
    assert(pipe != NULL);
    assert(arg != NULL);
    assert(input != NULL);
    assert(is_input_end != NULL);
    assert(output != NULL);

    TestMemoryPipeCopy *sink = (TestMemoryPipeCopy*)arg;
    sink->MoveToBuffer(input);
    *is_input_end = cont_buf_read_is_empty(input);
    return 0;
  }

static void
free_sink(void *arg) {
  UNUSED(arg);
}

TestMemoryPipeCopy::TestMemoryPipeCopy(struct event_pipe *source, size_t size) {
  assert(0 == cont_buf_create(size, &_buffer));

  EMPTY_STRUCT(event_pipe_config, pipe_conf);
  pipe_conf.arg = this;
  pipe_conf.arg_free = free_sink;
  assert(0 == mem_strdup("memory copy pipe", &pipe_conf.name));
  pipe_conf.buffer = _buffer;
  pipe_conf.on_read = on_read;
  assert(0 == event_pipe_create(source, &pipe_conf, &_pipe));
}
