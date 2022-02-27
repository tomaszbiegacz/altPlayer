#pragma once
#include "../event/event_pipe.h"
#include "pcm_spec.h"

/**
 * @brief Handle of event pcm pipe that can be used to read
 * decoded PCM stream parameters
 */
struct event_pcm;

//
// Setup
//

error_t
event_pcm_create(
  struct event_pipe *source,
  struct event_pipe_config *config,
  struct event_pcm **pcm_r,
  struct pcm_spec_builder **spec_r);

//
// Query
//

struct event_pipe*
event_pcm_get_pipe(struct event_pcm *pcm);

error_t
event_pcm_get_spec(struct event_pcm *pcm, struct pcm_spec **spec_r);
