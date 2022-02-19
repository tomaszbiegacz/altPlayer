#pragma once
#include "../SharedTestFixture.h"

extern "C" {
  #include "event/event_sink.h"
  #include "event/event_trigger.h"
}

class TestMemorySink {
  struct cont_buf *_buffer;
  struct event_trigger *_event;
  struct event_sink *_sink;

public:
  TestMemorySink(struct event_pipe *source, size_t size);

  void Dispose() {
    cont_buf_release(&_buffer);
    event_trigger_release(&_event);
  }

  void RegisterWrite() {
    assert(0 == event_trigger_activate(_event));
  }

  void Write() {
    assert(0 == event_sink_write(_sink));
  }

  void MoveToBuffer(struct cont_buf_read *input) {
    size_t inputSize = cont_buf_get_unread_size(input);
    assert(inputSize == cont_buf_move(_buffer, input));
  }

  struct event_sink* GetSink() {
    return _sink;
  }

  bool IsDataEqual(const char* value) {
    size_t value_size = strlen(value);
    char* data;
    size_t count = cont_buf_read_array_begin(
      cont_buf_read(_buffer),
      1, (const void**)&data, value_size + 1);
    if (count != value_size) {
      return false;
    }
    for(size_t i=0; i<count; ++i) {
      if (data[i] != value[i]) {
        return false;
      }
    }
    return true;
  }
};
