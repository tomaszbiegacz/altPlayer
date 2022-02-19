#include "TestHeaderPipe.h"

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
    assert(output == NULL);

    TestHeaderPipe *p = (TestHeaderPipe*)arg;
    p->ValidateHeader(input);
    *is_input_end = true;
    return 0;
  }

static void
free_sink(void *arg) {
  UNUSED(arg);
}

TestHeaderPipe::TestHeaderPipe(struct event_pipe *source, const char *header) {
  _header = header;

  EMPTY_STRUCT(event_pipe_config, pipe_conf);
  pipe_conf.arg = this;
  pipe_conf.arg_free = free_sink;
  assert(0 == mem_strdup("header pipe", &pipe_conf.name));
  pipe_conf.on_read = on_read;
  assert(0 == event_pipe_create(source, &pipe_conf, &_pipe));
  event_pipe_set_read_lowmark(_pipe, strlen(header));
}
