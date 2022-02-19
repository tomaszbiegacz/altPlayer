#pragma once
#include "event_loop.h"

/**
 * @brief Trigger event processing in the pipe.
 */
struct event_trigger;

typedef void event_on_trigger(void* arg);

//
// Setup
//

error_t
event_trigger_create(
  struct event_base *loop,
  void *arg,
  event_on_trigger *on_trigger,
  struct event_trigger **result_r);

void
event_trigger_release(struct event_trigger **result_r);

//
// Commands
//

error_t
event_trigger_activate(struct event_trigger *result);
