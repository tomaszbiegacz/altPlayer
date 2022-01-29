#pragma once
#include "shrdef.h"

typedef struct event_base events_loop;

//
// Setup
//

error_t
event_loop_global_init();

void
event_loop_global_release();


error_t
event_loop_create(struct event_base **result_r);

void
event_loop_release(struct event_base **result_r);

//
// Commands
//

error_t
event_loop_run(struct event_base *loop);
