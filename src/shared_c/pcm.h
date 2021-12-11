#ifndef PLAYER_PCM_H_
#define PLAYER_PCM_H_

#include "io.h"

struct pcm_spec {
  unsigned short channels_count;
  unsigned int samples_per_sec;
  unsigned short bits_per_sample;
  bool is_big_endian;
  bool is_signed;
  size_t samples_count;
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

struct timespec
pcm_spec_get_samples_time(const struct pcm_spec *params, size_t samples_count);

struct timespec
pcm_spec_get_total_time(const struct pcm_spec *params);

inline static void
pcm_spec_log(const char *block_name, const struct pcm_spec *params) {
  assert(params != NULL);
  if (log_is_verbose()) {
    log_verbose(
      "%s: PCM channels %d, samples/s %d, bts %d, isBigEndian %d, isSigned %d",
      block_name,
      params->channels_count,
      params->samples_per_sec,
      params->bits_per_sample,
      params->is_big_endian,
      params->is_signed);
  }
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
  struct pcm_spec spec;
  struct io_buffer dest;
  size_t block_size;

  pcm_decoder_decode_once_f decode_once;
  pcm_decoder_release_f release;
};

static inline struct timespec
pcm_decoder_get_total_time(const struct pcm_decoder *dec) {
  assert(dec != NULL);
  return pcm_spec_get_total_time(&dec->spec);
}

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

static inline bool
pcm_decoder_is_source_buffer_full(struct pcm_decoder *dec) {
  assert(dec != NULL);
  return io_rf_stream_is_buffer_full(dec->src);
}

static inline size_t
pcm_decoder_get_source_buffer_unread_size(struct pcm_decoder *dec) {
  assert(dec != NULL);
  return io_rf_stream_get_unread_buffer_size(dec->src);
}

static inline bool
pcm_decoder_is_source_buffer_ready_to_read(struct pcm_decoder *dec) {
  assert(dec != NULL);
  return io_rf_stream_get_unread_buffer_size(dec->src) > 0;
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
  return io_buffer_get_available_size(&dec->dest) < dec->block_size;
}

static inline size_t
pcm_decoder_get_output_buffer_frames_count(struct pcm_decoder *dec) {
  assert(dec != NULL);
  return io_buffer_get_unread_size(&dec->dest) / pcm_frame_size(&dec->spec);
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

/**
 * @brief WAV format decoder implementation
 *
 */
error_t
pcm_decoder_wav_open(
  struct io_rf_stream *src,
  size_t buffer_size,
  struct pcm_decoder **decoder);

#endif
