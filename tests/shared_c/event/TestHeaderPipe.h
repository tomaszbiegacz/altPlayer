#pragma once
#include "../SharedTestFixture.h"

extern "C" {
  #include "event/event_pipe.h"
}

class TestHeaderPipe {
  const char *_header;
  struct event_pipe *_pipe;

public:
  TestHeaderPipe(struct event_pipe *source, const char *header);

  struct event_pipe* GetPipe() {
    return _pipe;
  }

  void ValidateHeader(struct cont_buf_read *input) {
    size_t header_size = strlen(_header);
    const char *data = NULL;
    size_t count = cont_buf_read_array_begin(
      input,
      1, (const void**)&data, header_size);
    assert(0 == strncmp(_header, data, count));
    cont_buf_read_array_commit(input, 1, count);
  }
};
