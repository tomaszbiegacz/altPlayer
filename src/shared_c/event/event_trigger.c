#include <event.h>
#include "../mem.h"
#include "../log.h"
#include "event_trigger.h"

struct event_trigger {
  struct event *event;
  event_on_trigger *on_trigger;
  void *arg;
};

static void
event_trigger_on(evutil_socket_t s, int16_t f, void *arg) {
  UNUSED(s);
  UNUSED(f);
  assert(arg != NULL);

  struct event_trigger *result = (struct event_trigger*)arg;
  result->on_trigger(result->arg);
}

error_t
event_trigger_create(
  struct event_base *loop,
  void *arg,
  event_on_trigger *on_trigger,
  struct event_trigger **result_r) {
    assert(loop != NULL);
    assert(on_trigger != NULL);
    assert(result_r != NULL);

    struct event_trigger *result = NULL;
    error_t error_r = mem_calloc(
      "event_trigger",
      sizeof(struct event_trigger),
      (void**)&result); // NOLINT

    if (error_r == 0) {
      result->on_trigger = on_trigger;
      result->arg = arg;

      result->event = event_new(
        loop,
        -1, 0,
        event_trigger_on, result);
      if (result->event == NULL) {
        error_r = errno;
        log_error("Creating event failed with %s", strerror(error_r));
      }
    }
    if (error_r == 0) {
      error_r = event_trigger_activate(result);
    }

    if (error_r == 0) {
      *result_r = result;
    } else {
      event_trigger_release(&result);
    }
    return error_r;
  }

void
event_trigger_release(struct event_trigger **result_r) {
  assert(result_r != NULL);

  struct event_trigger *result = *result_r;
  if (result != NULL) {
    event_free(result->event);
    free(result);
    *result_r = NULL;
  }
}

error_t
event_trigger_activate(struct event_trigger *result) {
  assert(result != NULL);

  EMPTY_STRUCT(timeval, timeout);
  error_t error_r = event_add(result->event, &timeout);
  if (error_r != 0) {
    log_error("Cannot add trigger event");
  }
  return error_r;
}
