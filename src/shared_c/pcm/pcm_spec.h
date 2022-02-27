#pragma once
#include "../log.h"

//
// sample - single value in PCM format
// frame - set of samples for each channel
//

/**
 * @brief PCM specification builder
 */
struct pcm_spec_builder;

/**
 * @brief PCM specification reader
 */
struct pcm_spec;

//
// Setup
//

error_t
pcm_spec_builder_create(struct pcm_spec_builder **result);

void
pcm_spec_builder_release(struct pcm_spec_builder **result);

void
pcm_spec_release(struct pcm_spec **result);

//
// Query
//

error_t
pcm_spec_get(struct pcm_spec_builder *builder, struct pcm_spec **result);

bool
pcm_spec_is_big_endian(const struct pcm_spec *params);

bool
pcm_spec_is_signed(const struct pcm_spec *params);

uint16_t
pcm_spec_get_channels_count(const struct pcm_spec *params);

uint16_t
pcm_spec_get_bytes_per_sample(const struct pcm_spec *params);

uint32_t
pcm_spec_get_samples_per_sec(const struct pcm_spec *params);

size_t
pcm_spec_get_frames_count(const struct pcm_spec *params);

inline static uint16_t
pcm_spec_get_bits_per_sample(const struct pcm_spec *params) {
  return pcm_spec_get_bytes_per_sample(params) * 8;
}

inline static uint16_t
pcm_spec_get_frame_size(const struct pcm_spec *params) {
  return pcm_spec_get_channels_count(params)
    * pcm_spec_get_bytes_per_sample(params);
}

struct timespec
pcm_spec_get_time_for_frames(
  const struct pcm_spec *params,
  size_t frames_count);

inline static size_t
pcm_spec_get_frames_count_for_time(
  const struct pcm_spec *params,
  size_t time_ms) {
    return pcm_spec_get_samples_per_sec(params) * time_ms / 1000;
  }

inline static struct timespec
pcm_spec_get_time(const struct pcm_spec *params) {
  return pcm_spec_get_time_for_frames(
    params,
    pcm_spec_get_frames_count(params));
}

inline static void
pcm_spec_log(const char *block_name, const struct pcm_spec *params) {
  if (log_is_verbose()) {
    log_verbose(
"%s: PCM %d ch, %d Hz, bitrate %d, %s %s, %d frames",
      block_name,
      pcm_spec_get_channels_count(params),
      pcm_spec_get_samples_per_sec(params),
      pcm_spec_get_bits_per_sample(params),
      pcm_spec_is_signed(params) ? "signed" : "unsigned",
      pcm_spec_is_big_endian(params) ? "big endian" : "little endian",
      pcm_spec_get_frames_count(params));
  }
}

//
// Commands
//

void
pcm_spec_set_big_endian(struct pcm_spec_builder *params);

void
pcm_spec_set_signed(struct pcm_spec_builder *params);

void
pcm_spec_set_channels_count(
  struct pcm_spec_builder *params,
  uint16_t value);

void
pcm_spec_set_bits_per_sample(
  struct pcm_spec_builder *params,
  uint16_t value);

void
pcm_spec_set_samples_per_sec(
  struct pcm_spec_builder *params,
  uint32_t value);

void
pcm_spec_set_frames_count(
  struct pcm_spec_builder *params,
  size_t value);

error_t
pcm_spec_set_data_size(
  struct pcm_spec_builder *params,
  size_t data_size);
