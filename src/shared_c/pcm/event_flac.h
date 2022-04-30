#pragma once
#include "event_pcm.h"

/**
 * @brief Create pipe for decoding flac
 */
error_t
event_flac_decode(
  struct event_pipe *source,
  struct event_pcm **result_r);
