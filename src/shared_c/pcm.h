#ifndef PLAYER_PCM_H_
#define PLAYER_PCM_H_

#include "io.h"

struct pcm_spec {
  unsigned short channels_count;
  unsigned int samples_per_sec;
  unsigned short bits_per_sample;
  size_t data_size;
};

inline static size_t
pcm_frame_size(const struct pcm_spec *params) {
  assert(params != NULL);
  assert(params->bits_per_sample % 8 == 0);
  return params->channels_count * params->bits_per_sample / 8;
}

inline static unsigned int
pcm_buffer_time_us(const struct pcm_spec *params, size_t size) {
  assert(params != NULL);
  return size / pcm_frame_size(params)  // frames in buffer
    * 1000 * 1000                       // resolution in us from s
    / params->samples_per_sec;
}

enum pcm_format {
  pcm_format_wav      = 1,
  pcm_format_flac     = 2,
};

error_t
pcm_guess_format(
  const char *file_name,
  enum pcm_format *format);

/**
 * @brief PCM stream decoder
 *
 */
struct pcm_decoder;

typedef error_t (*pcm_decoder_decode_once_f) (struct pcm_decoder *handler);

typedef void (*pcm_decoder_release_f) (struct pcm_decoder **handler);

struct pcm_decoder {
  struct io_rf_stream *src;
  struct io_buffer dest;
  struct pcm_spec spec;

  pcm_decoder_decode_once_f decode_once;
  pcm_decoder_release_f release;
};

static inline size_t
pcm_decoder_frame_size(struct pcm_decoder *dec) {
  assert(dec != NULL);
  return pcm_frame_size(&dec->spec);
}

static inline bool
pcm_decoder_is_source_empty(struct pcm_decoder *dec) {
  assert(dec != NULL);
  return io_rf_stream_is_empty(dec->src);
}

static inline bool
pcm_decoder_is_source_buffer_empty(struct pcm_decoder *dec) {
  assert(dec != NULL);
  return io_rf_stream_is_empty(dec->src);
}

static inline error_t
pcm_decoder_read_source(struct pcm_decoder *dec, int poll_timeout) {
  assert(dec != NULL);
  if(!io_rf_stream_is_eof(dec->src) && !io_rf_stream_is_buffer_full(dec->src)) {
    return io_rf_stream_read_with_poll(dec->src, poll_timeout);
  }
  return 0;
}

static inline bool
pcm_decoder_is_output_buffer_empty(struct pcm_decoder *dec) {
  assert(dec != NULL);
  return io_buffer_get_unread_size(&dec->dest) < pcm_frame_size(&dec->spec);
}

static inline bool
pcm_decoder_is_output_buffer_full(struct pcm_decoder *dec) {
  assert(dec != NULL);
  return io_buffer_get_available_size(&dec->dest) < pcm_frame_size(&dec->spec);
}

static inline error_t
pcm_decoder_decode_once(struct pcm_decoder *dec) {
  assert(dec != NULL);
  return dec->decode_once(dec);
}

static inline void
pcm_decoder_decode_release(struct pcm_decoder **dec) {
  assert(dec != NULL);
  (*dec)->release(dec);
}

error_t
pcm_decoder_wav_open(
  struct io_rf_stream *src,
  size_t buffer_size,
  struct pcm_decoder **decoder);

#endif
