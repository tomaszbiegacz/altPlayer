#pragma once
#include "event_pcm.h"

/**
 * @brief Create pipe for decoding wav
 */
error_t
event_wav_decode(
  struct event_pipe *source,
  struct event_pcm **result_r);

/**
 * @brief Create pipe for encoding pcm stream into wav
 */
error_t
event_wav_encode(
  struct event_pcm *source,
  struct event_pipe **result_r);
