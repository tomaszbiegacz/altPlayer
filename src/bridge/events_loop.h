#ifndef PLAYER_EVENTS_LOOP_H_
#define PLAYER_EVENTS_LOOP_H_

#include "shrdef.h"

typedef struct event_base events_loop;
typedef struct evdns_base event_dns;

//
// Setup
//

error_t
events_loop_global_init();

void
events_loop_global_release();


error_t
events_loop_create(events_loop **result_r);

void
events_loop_release(events_loop **result_r);


error_t
event_dns_create(
  events_loop *loop,
  event_dns **result_r);

void
event_dns_release(event_dns **result_r);

//
// Commands
//

error_t
events_loop_run(events_loop *loop);

#endif
