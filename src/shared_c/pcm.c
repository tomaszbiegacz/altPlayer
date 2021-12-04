#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "pcm.h"

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

error_t
pcm_guess_format(
  const char *file_name,
  enum pcm_format *format) {
    assert(format != NULL);
    const char *ext = get_filename_ext(file_name);

    if (strcasecmp(ext, "wav") == 0) {
      *format = pcm_format_wav;
      return 0;
    }

    if (strcasecmp(ext, "flac") == 0) {
      *format = pcm_format_flac;
      return 0;
    }

    return EINVAL;
  }

static error_t
validate_wav_header(
  struct io_rf_stream *stream,
  size_t* fmt_length) {
    struct wav_header *header;
    error_t error_r = io_rf_stream_read(
      stream,
      sizeof(struct wav_header), (void**)&header); //NOLINT

    if (error_r == 0 && memcmp(header->riff_marker, "RIFF", 4) != 0) {
      log_error("File is not in WAV format (1).");
      error_r = EINVAL;
    }
    if (error_r == 0 && memcmp(header->form_type, "WAVE", 4) != 0) {
      log_error("File is not in WAV format (2).");
      error_r = EINVAL;
    }
    if (error_r == 0 && memcmp(header->fmt_chunk_marker, "fmt ", 4) != 0) {
      log_error("File is not in WAV format (3).");
      error_r = EINVAL;
    }
    if (error_r == 0) {
      *fmt_length = header->fmt_length;
    }
    return error_r;
  }

static error_t
validate_wav_fmt_chunk_header(
  struct io_rf_stream *stream,
  size_t fmt_length,
  struct pcm_spec *result) {
    error_t error_r = 0;
    if (fmt_length < sizeof(struct wav_fmt_chunk_header)) {
      log_error(
        "memory allocated for fmt_header is too small: %d",
        fmt_length);
      error_r = EINVAL;
    }

    struct wav_fmt_chunk_header *header;
    if (error_r == 0) {
      error_r = io_rf_stream_read(
        stream,
        fmt_length, (void**)&header); //NOLINT
    }
    if (error_r == 0 && header->fmt_format_type != 1) {
      log_error("File is not in PCM WAV format (1).");
      error_r = EINVAL;
    }
    if (error_r == 0) {
      const unsigned int samples_per_sec = header->samples_per_sec;
      const unsigned int bits_per_sample = header->bits_per_sample;
      const unsigned int channels_count = header->n_channels;

      const unsigned int exp_avg_bytes_per_sec =
        samples_per_sec * channels_count * bits_per_sample / 8;
      if (header->avg_bytes_per_sec != exp_avg_bytes_per_sec) {
        log_error("File is not in PCM WAV format (3).");
        error_r = EINVAL;
      }
      if (error_r == 0
          && header->block_align != channels_count * bits_per_sample / 8) {
        log_error("File is not in PCM WAV format (4).");
        error_r = EINVAL;
      }

      result->channels_count = channels_count;
      result->samples_per_sec = samples_per_sec;
      result->bits_per_sample = bits_per_sample;
    }
    return error_r;
  }

static error_t
validate_wav_data_chunk_header(
  struct io_rf_stream *stream,
  struct pcm_spec *result) {
    struct wav_data_chunk_header *header;
    error_t error_r = io_rf_stream_read(
      stream,
      sizeof(struct wav_data_chunk_header), (void**)&header);  //NOLINT

    if (error_r == 0 && memcmp(header->data_marker, "data", 4) != 0) {
      log_error("File is not in PCM WAV format (5).");
      error_r = EINVAL;
    }
    if (error_r == 0) {
      result->data_size = header->data_size;
    }
    return error_r;
  }

static error_t
pcm_validate_wav_content(
  struct io_rf_stream *stream,
  struct pcm_spec *result) {
    assert(result != NULL);

    size_t fmt_length;
    error_t error_r = validate_wav_header(stream, &fmt_length);
    if (error_r == 0) {
      error_r = validate_wav_fmt_chunk_header(stream, fmt_length, result);
    }
    if (error_r == 0) {
      error_r = validate_wav_data_chunk_header(stream, result);
    }
    return error_r;
  }

struct pcm_decoder_wav {
  struct pcm_decoder base;
};

static error_t
pcm_decoder_wav_decode_once(struct pcm_decoder *handler) {
  assert(handler != NULL);
  assert(io_rf_stream_get_unread_buffer_size(handler->src) > 0);
  assert(!io_buffer_is_full(&handler->dest));

  void* data;
  size_t count = io_rf_stream_read_array(
    handler->src, 1, &data, io_buffer_get_available_size(&handler->dest));
  assert(io_buffer_try_write(&handler->dest, count, data));
  return 0;
}

static void
pcm_decoder_wav_release(struct pcm_decoder **handler) {
  assert(handler != NULL);
  struct pcm_decoder *to_release = *handler;
  if (to_release != NULL) {
    io_buffer_free(&to_release->dest);
    free(to_release);
    *handler = NULL;
  }
}

error_t
pcm_decoder_wav_open(
  struct io_rf_stream *src,
  size_t buffer_size,
  struct pcm_decoder **decoder) {
    assert(!io_rf_stream_is_empty(src));
    struct pcm_decoder_wav *result = calloc(1, sizeof(struct pcm_decoder_wav));
    error_t error_r = 0;
    if (result == NULL) {
      log_error("Insufficient memory for 'pcm_decoder_wav'");
      error_r = ENOMEM;
    }
    if (error_r == 0) {
      error_r = pcm_validate_wav_content(src, &result->base.spec);
    }
    if (error_r == 0) {
      error_r = io_buffer_alloc(buffer_size, &result->base.dest);
    }
    if (error_r == 0) {
      result->base.src = src;
      result->base.decode_once = &pcm_decoder_wav_decode_once;
      result->base.release = &pcm_decoder_wav_release;
      *decoder = (struct pcm_decoder*)result;
    } else {
      pcm_decoder_wav_release((struct pcm_decoder**)&result);
    }
    return error_r;
  }
