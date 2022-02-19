#pragma once
#include "../SharedTestFixture.h"

extern "C" {
  #include "event/event_pipe.h"
}

class TestMemoryPipeCopy {
  struct cont_buf *_buffer;
  struct event_pipe *_pipe;

public:
  TestMemoryPipeCopy(struct event_pipe *source, size_t size);

  void MoveToBuffer(struct cont_buf_read *input) {
    cont_buf_move(_buffer, input);
  }

  struct event_pipe* GetPipe() {
    return _pipe;
  }
};