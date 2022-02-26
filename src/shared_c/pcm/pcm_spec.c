#include "../mem.h"
#include "pcm_spec.h"

struct pcm_spec {
  bool is_big_endian;
  bool is_signed;
  uint16_t channels_count;
  uint16_t bytes_per_sample;
  uint32_t samples_per_sec;
  size_t frames_count;
};

struct pcm_spec_builder {
  struct pcm_spec spec;
};

error_t
pcm_spec_builder_create(struct pcm_spec_builder **result) {
  assert(result != NULL);

  return mem_calloc(
    "pcm_spec_builder",
    sizeof(struct pcm_spec_builder),
    (void**)result); // NOLINT
}

void
pcm_spec_builder_release(struct pcm_spec_builder **result) {
  mem_free((void**)result); // NOLINT
}

void
pcm_spec_release(struct pcm_spec **result) {
  mem_free((void**)result); // NOLINT
}

error_t
pcm_spec_get(struct pcm_spec_builder *builder, struct pcm_spec **result) {
  assert(builder != NULL);
  assert(builder->spec.bytes_per_sample > 0);
  assert(builder->spec.channels_count > 0);
  assert(builder->spec.samples_per_sec > 0);
  assert(builder->spec.frames_count > 0);

  error_t error_r = mem_calloc(
    "pcm_spec",
    sizeof(struct pcm_spec),
    (void**)result);  // NOLINT
  if (error_r == 0) {
    **result = builder->spec;
  }
  return error_r;
}

bool
pcm_spec_is_big_endian(const struct pcm_spec *params) {
  assert(params != NULL);
  return params->is_big_endian;
}

bool
pcm_spec_is_signed(const struct pcm_spec *params) {
  assert(params != NULL);
  return params->is_signed;
}

uint16_t
pcm_spec_get_channels_count(const struct pcm_spec *params) {
  assert(params != NULL);
  return params->channels_count;
}

uint16_t
pcm_spec_get_bytes_per_sample(const struct pcm_spec *params) {
  assert(params != NULL);
  return params->bytes_per_sample;
}

uint32_t
pcm_spec_get_samples_per_sec(const struct pcm_spec *params) {
  assert(params != NULL);
  return params->samples_per_sec;
}

size_t
pcm_spec_get_frames_count(const struct pcm_spec *params) {
  assert(params != NULL);
  return params->frames_count;
}

struct timespec
pcm_spec_get_time_for_frames(
  const struct pcm_spec *params,
  size_t frames_count) {
    assert(params != NULL);
    struct timespec result = {0};
    result.tv_sec = frames_count / params->samples_per_sec;
    result.tv_nsec = 1000l * 1000l * 1000l  // nano in second
      * (frames_count - result.tv_sec * params->samples_per_sec)
      / params->samples_per_sec;
    return result;
  }

void
pcm_spec_set_big_endian(struct pcm_spec_builder *params) {
  assert(params != NULL);
  params->spec.is_big_endian = true;
}

void
pcm_spec_set_signed(struct pcm_spec_builder *params) {
  assert(params != NULL);
  params->spec.is_signed = true;
}

void
pcm_spec_set_channels_count(
  struct pcm_spec_builder *params,
  uint16_t value) {
    assert(params != NULL);
    params->spec.channels_count = value;
  }

void
pcm_spec_set_bytes_per_sample(
  struct pcm_spec_builder *params,
  uint16_t value) {
    assert(params != NULL);
    params->spec.bytes_per_sample = value;
  }

void
pcm_spec_set_samples_per_sec(
  struct pcm_spec_builder *params,
  uint32_t value) {
    assert(params != NULL);
    params->spec.samples_per_sec = value;
  }

void
pcm_spec_set_frames_count(
  struct pcm_spec_builder *params,
  size_t value) {
    assert(params != NULL);
    params->spec.frames_count = value;
  }
