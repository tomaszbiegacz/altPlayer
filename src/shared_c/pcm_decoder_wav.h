#ifndef PLAYER_PCM_DECODER_WAV_H_
#define PLAYER_PCM_DECODER_WAV_H_

#include "pcm_decoder.h"

/**
 * @brief WAV format decoder implementation
 *
 */
error_t
pcm_decoder_wav_open(
  struct io_rf_stream *src,
  struct pcm_decoder **decoder);

#endif
