#ifndef PLAYER_FLAC_H_
#define PLAYER_FLAC_H_

#include "pcm.h"

/**
 * @brief FLAC formet decoder implementation
 *
 */
error_t
pcm_decoder_flac_open(
  struct io_rf_stream *src,
  size_t buffer_size,
  struct pcm_decoder **decoder);

#endif