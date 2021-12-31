#ifndef PLAYER_PCM_DECODER_H_
#define PLAYER_PCM_DECODER_H_

#include <stdlib.h>
#include "io_rf_stream.h"
#include "pcm_spec.h"

/**
 * @brief PCM stream decoder
 *
 */
struct pcm_decoder;

typedef error_t (*pcm_decoder_decode_once_f) (struct pcm_decoder *handler);

typedef void (*pcm_decoder_release_f) (struct pcm_decoder *handler);

struct pcm_decoder {
  struct io_rf_stream *src;
  struct pcm_spec spec;
  struct io_buffer dest;
  size_t block_size;

  pcm_decoder_decode_once_f decode_once;
  pcm_decoder_release_f release;
};


//
// Setup
//

error_t
pcm_decoder_allocate_output_buffer(
  struct pcm_decoder *dec,
  size_t frames_count);

void
pcm_decoder_release(struct pcm_decoder **dec);

//
// Query
//

static inline const struct pcm_spec*
pcm_decoder_get_spec(const struct pcm_decoder *dec) {
  assert(dec != NULL);
  return &dec->spec;
}

static inline bool
pcm_decoder_is_source_empty(const struct pcm_decoder *dec) {
  assert(dec != NULL);
  return io_rf_stream_is_empty(dec->src);
}

static inline const struct io_buffer*
pcm_decoder_get_source_buffer(const struct pcm_decoder *dec) {
  assert(dec != NULL);
  return io_rf_stream_get_buffer(dec->src);
}

static inline bool
pcm_decoder_can_source_buffer_fillup(const struct pcm_decoder *dec) {
  bool is_full = io_buffer_is_full(pcm_decoder_get_source_buffer(dec));
  bool is_eof = io_rf_stream_is_eof(dec->src);
  return !is_full && !is_eof;
}

static inline bool
pcm_decoder_is_source_ready_to_fillup(const struct pcm_decoder *dec) {
  const struct io_buffer* buf = pcm_decoder_get_source_buffer(dec);
  size_t threshold = io_buffer_get_allocated_size(buf) / 2;
  return io_buffer_get_available_size(buf) > threshold;
}

static inline bool
pcm_decoder_is_output_buffer_full(const struct pcm_decoder *dec) {
  assert(dec != NULL);
  return io_buffer_get_available_size(&dec->dest) < dec->block_size;
}

static inline size_t
pcm_decoder_get_output_buffer_frames_count(const struct pcm_decoder *dec) {
  assert(dec != NULL);
  return
    io_buffer_get_unread_size(&dec->dest) / pcm_spec_get_frame_size(&dec->spec);
}

static inline bool
pcm_decoder_is_empty(const struct pcm_decoder *dec) {
  return pcm_decoder_is_source_empty(dec)
    && pcm_decoder_get_output_buffer_frames_count(dec) == 0;
}


//
// Commands
//

error_t
pcm_decoder_read_source_try(struct pcm_decoder *dec, int poll_timeout);

static inline error_t
pcm_decoder_decode_once(struct pcm_decoder *dec) {
  assert(dec != NULL);
  return dec->decode_once(dec);
}

inline static bool
pcm_decoder_try_write_sample(struct pcm_decoder *dec, int32_t sample) {
  assert(dec != NULL);
  return io_buffer_try_write(
    &dec->dest,
    pcm_spec_get_bytes_per_sample(&dec->spec),
    &sample);
}

#endif
