#pragma once
#include "../event/event_loop.h"
#include "http2_request.h"

error_t
event_pipe_from_http2(
  events_loop *loop,
  struct http2_request *request,
  size_t buffer_size,
  struct event_pipe **result_r);

// get response headers, processing stage etc
