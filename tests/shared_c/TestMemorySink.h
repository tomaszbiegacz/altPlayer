#pragma once
#include "SharedTestFixture.h"

extern "C" {
  #include "event_sink.h"
  #include "event_trigger.h"
}

class TestMemorySink {
  struct cont_buf *_buffer;
  struct event_trigger *_event;
  struct event_sink *_sink;

public:
  TestMemorySink(struct event_pipe *source, size_t size);
  ~TestMemorySink() {
    cont_buf_release(&_buffer);
    event_trigger_release(&_event);
    event_sink_release(&_sink);
  }

  struct cont_buf_read* GetBuffer() {
    return cont_buf_read(_buffer);
  }

  void RegisterWrite() {
    EXPECT_EQ(0, event_trigger_activate(_event));
  }

  void Write() {
    EXPECT_EQ(0, event_sink_write(_sink));
  }

  void MoveToBuffer(struct cont_buf_read *input) {
    size_t inputSize = cont_buf_get_unread_size(input);
    EXPECT_EQ(inputSize, cont_buf_move(_buffer, input));
  }
};
