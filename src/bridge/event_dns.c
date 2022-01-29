#include <event2/dns.h>
#include "event_dns.h"
#include "log.h"

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
