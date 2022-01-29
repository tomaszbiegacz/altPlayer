#pragma once
#include "pcm_decoder.h"

/**
 * @brief FLAC formet decoder implementation
 *
 */
error_t
pcm_decoder_flac_open(
  struct io_rf_stream *src,
  struct pcm_decoder **decoder);
