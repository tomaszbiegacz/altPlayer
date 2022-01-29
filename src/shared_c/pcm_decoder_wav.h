#pragma once
#include "pcm_decoder.h"

/**
 * @brief WAV format decoder implementation
 *
 */
error_t
pcm_decoder_wav_open(
  struct io_rf_stream *src,
  struct pcm_decoder **decoder);
