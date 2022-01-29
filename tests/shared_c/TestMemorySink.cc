#include "TestMemorySink.h"

extern "C" {
  #include "mem.h"
}

static error_t
on_register_trigger(struct event_sink *sink, void *arg) {
  UNUSED(sink);
  assert(NULL != arg);
  struct TestMemorySink *r = (TestMemorySink*)arg;
  r->RegisterWrite();
  return 0;
}

static void
on_trigger(void *arg) {
  TestMemorySink *r = (TestMemorySink*)arg;
  r->Write();
}

static error_t
on_write(
  struct event_sink *sink,
  void *arg,
  struct cont_buf_read *input) {
    UNUSED(sink);
    assert(NULL != arg);
    struct TestMemorySink *r = (TestMemorySink*)arg;
    r->MoveToBuffer(input);
    return 0;
  }

TestMemorySink::TestMemorySink(struct event_pipe *source, size_t size) {
  struct event_base *loop = event_pipe_get_loop(source);

  assert(0 == cont_buf_create(size, &_buffer));

  EMPTY_STRUCT(event_sink_config, sink_conf);
  sink_conf.arg = this;
  assert(0 == mem_strdup("memory sink", &sink_conf.name));
  sink_conf.on_write = on_write;
  sink_conf.on_register_event = on_register_trigger;
  assert(0 == event_sink_create(source, &sink_conf, &_sink));

  assert(0 == event_trigger_create(loop, this, on_trigger, &_event));
}
