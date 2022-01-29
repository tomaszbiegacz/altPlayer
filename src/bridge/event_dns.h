#ifndef PLAYER_EVENT_DNS_H_
#define PLAYER_EVENT_DNS_H_

#include "events_loop.h"

typedef struct evdns_base event_dns;

//
// Setup
//

error_t
event_dns_create(
  events_loop *loop,
  event_dns **result_r);

void
event_dns_release(event_dns **result_r);

#endif
