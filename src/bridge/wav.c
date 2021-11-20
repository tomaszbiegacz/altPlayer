#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include "log.h"
#include "wav.h"

/**
 * RIFF WAV file header, see
 * http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html
 */

struct wav_header {
  unsigned char riff_marker[4];       // "RIFF" string
  uint32_t overall_size;              // overall size of file in bytes
  unsigned char form_type[4];         // "WAVE" string

  unsigned char fmt_chunk_marker[4];  // "fmt" string with trailing null char
  uint32_t fmt_length;                // 16, 18 or 40: length of fmt chunk
};

struct wav_fmt_chunk_header {
  uint16_t fmt_format_type;     // format type: 1-PCM, ...
  uint16_t n_channels;          // number of channels, i.e. 2
  uint32_t samples_per_sec;     // sampling rate, i.e. 44100
  uint32_t avg_bytes_per_sec;   // sample_rate*num_channels*bits_per_sample/8
  uint16_t block_align;         // num_channels * bits_per_sample / 8
  uint16_t bits_per_sample;     // bits per sample, i.e. 16
};

struct wav_data_chunk_header {
  unsigned char data_marker[4];       // "data" string
  uint32_t data_size;                 // size of the data section
};


static error_t
validate_wav_header(
  struct io_memory_block *content,
  struct wav_fmt_chunk_header **fmt_header,
  struct wav_data_chunk_header **data_header) {
  const size_t memory_block_min_size =
    sizeof(struct wav_header) +
    sizeof(struct wav_fmt_chunk_header) +
    sizeof(struct wav_data_chunk_header);

  if (content->size < memory_block_min_size) {
    log_error("File is too small for WAV header.");
    return EINVAL;
  }

  struct wav_header *header = content->data;
  if (memcmp(header->riff_marker, "RIFF", 4) != 0) {
    log_error("File is not in WAV format (1).");
    return EINVAL;
  }
  if (memcmp(header->form_type, "WAVE", 4) != 0) {
    log_error("File is not in WAV format (2).");
    return EINVAL;
  }
  if (memcmp(header->fmt_chunk_marker, "fmt ", 4) != 0) {
    log_error("File is not in WAV format (3).");
    return EINVAL;
  }
  if (header->fmt_length < 16) {
    log_error("File is not in WAV format (4).");
    return EINVAL;
  }

  char* data = content->data;
  *fmt_header = (struct wav_fmt_chunk_header*) (
    data + sizeof(struct wav_header));
  *data_header = (struct wav_data_chunk_header*) (
    data + sizeof(struct wav_header) + header->fmt_length);
  return 0;
}

static error_t
validate_fmt_header(
  struct wav_fmt_chunk_header *header,
  struct wav_pcm_content *result) {
  if (header->fmt_format_type != 1) {
    log_error("File is not in PCM WAV format (1).");
    return EINVAL;
  }
  const unsigned int samples_per_sec = header->samples_per_sec;
  const unsigned int bits_per_sample = header->bits_per_sample;
  const unsigned int channels_count = header->n_channels;

  const unsigned int exp_avg_bytes_per_sec =
    samples_per_sec * channels_count * bits_per_sample / 8;
  if (header->avg_bytes_per_sec != exp_avg_bytes_per_sec) {
    log_error("File is not in PCM WAV format (3).");
    return EINVAL;
  }
  if (header->block_align != channels_count * bits_per_sample / 8) {
    log_error("File is not in PCM WAV format (4).");
    return EINVAL;
  }

  result->channels_count = channels_count;
  result->samples_per_sec = samples_per_sec;
  result->bits_per_sample = bits_per_sample;
  return 0;
}

static error_t
validate_data_header(
  struct wav_data_chunk_header *header,
  struct wav_pcm_content *result) {
  if (memcmp(header->data_marker, "data", 4) != 0) {
    log_error("File is not in PCM WAV format (5).");
    return EINVAL;
  }

  char* data = (char*)header + sizeof(struct wav_data_chunk_header);
  result->pcm = (struct io_memory_block) {
    .size = header->data_size,
    .data = data
  };
  return 0;
}

error_t
wav_validate_pcm_content(
  struct io_memory_block *content,
  struct wav_pcm_content *result) {
  assert(content != NULL);
  assert(!io_memory_block_is_empty(content));
  assert(result != NULL);

  error_t error_result;
  struct wav_fmt_chunk_header *fmt_header;
  struct wav_data_chunk_header *data_header;

  error_result = validate_wav_header(content, &fmt_header, &data_header);
  if (error_result != 0)
    return error_result;

  error_result = validate_fmt_header(fmt_header, result);
  if (error_result != 0)
    return error_result;

  error_result = validate_data_header(data_header, result);
  if (error_result != 0)
    return error_result;

  // all is ok
  log_verbose(
    "WAV parameters: channels: %d, %d samples/s, %d bitrate",
    result->channels_count,
    result->samples_per_sec,
    result->bits_per_sample);
  return 0;
}
