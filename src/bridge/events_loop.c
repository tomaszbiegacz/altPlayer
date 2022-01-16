#include <assert.h>
#include <event.h>
#include <event2/event.h>
#include <event2/dns.h>
#include "events_loop.h"
#include "log.h"

static bool _is_global_init = false;

static void
event_log(int severity, const char *msg) {
  switch (severity) {
    case EVENT_LOG_DEBUG:
      log_verbose(msg);
      break;

    case EVENT_LOG_MSG:
      log_info(msg);
      break;

    case EVENT_LOG_WARN:
    case EVENT_LOG_ERR:
      log_error(msg);
      break;

    default:
      log_error("Unknown severity %d for error %s", severity, msg);
      break;
  }
}

error_t
events_loop_global_init() {
  if (!_is_global_init) {
    if (log_is_verbose()) {
      log_verbose("Libevent version: %s", event_get_version());
    }
    event_set_log_callback(event_log);
    _is_global_init = true;
  }
  return 0;
}

void
events_loop_global_release() {
  libevent_global_shutdown();
}

error_t
events_loop_create(events_loop **result_r) {
  assert(result_r != NULL);

  struct event_base *evbase = event_base_new();
  if (evbase == NULL) {
    log_error("EVENT: cannot create event base");
    return EINVAL;
  }

  *result_r = evbase;
  return 0;
}

void
events_loop_release(events_loop **result_r) {
  assert(result_r != NULL);

  struct event_base* result = *result_r;
  if (result != NULL) {
    event_base_free(result);
    *result_r = NULL;
  }
}

error_t
events_loop_run(events_loop *loop) {
  assert(loop != NULL);

  error_t error_r = event_base_loop(loop, 0);
  if (error_r == 1) {
    log_verbose("EVENT: loop finished because there are no more events.");
    error_r = 0;
  }
  return error_r;
}

error_t
event_dns_create(
  events_loop *loop,
  event_dns **result_r) {
    assert(loop != NULL);
    assert(result_r != NULL);

    struct evdns_base *result = evdns_base_new(loop, 1);
    if (result == NULL) {
      log_error("EVENT: cannot create event dns");
      return EINVAL;
    }

    *result_r = result;
    return 0;
  }

void
event_dns_release(event_dns **result_r) {
  assert(result_r != NULL);

  struct evdns_base* result = *result_r;
  if (result != NULL) {
    evdns_base_free(result, 1);
    *result_r = NULL;
  }
}
