#include <stdint.h>
#include <strings.h>
#include "pcm_decoder_wav.h"

/**
 * RIFF WAV file header, see
 * http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html
 */

struct wav_header {
  unsigned char riff_marker[4];       // "RIFF" or "RIFX" string
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
  struct io_rf_stream *stream,
  struct pcm_spec *spec,
  size_t* fmt_length) {
    struct wav_header *header;
    error_t error_r = io_rf_stream_read(
      stream,
      sizeof(struct wav_header), (void**)&header); //NOLINT

    if (error_r == 0) {
      if (memcmp(header->riff_marker, "RIFF", 4) == 0) {
        spec->is_big_endian = false;
      } else if (memcmp(header->riff_marker, "RIFX", 4) == 0) {
        spec->is_big_endian = true;
      } else {
        log_error("WAV: invalid header (1)");
        error_r = EINVAL;
      }
    }
    if (error_r == 0 && memcmp(header->form_type, "WAVE", 4) != 0) {
      log_error("WAV: invalid header (2).");
      error_r = EINVAL;
    }
    if (error_r == 0 && memcmp(header->fmt_chunk_marker, "fmt ", 4) != 0) {
      log_error("WAV: invalid header (3).");
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
        "WAV: memory allocated for fmt_header is too small: %d",
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
      log_error("WAV: invalid header (3).");
      error_r = EINVAL;
    }
    if (error_r == 0) {
      const unsigned int samples_per_sec = header->samples_per_sec;
      const unsigned int bits_per_sample = header->bits_per_sample;
      const unsigned int channels_count = header->n_channels;

      const unsigned int exp_avg_bytes_per_sec =
        samples_per_sec * channels_count * bits_per_sample / 8;
      if (header->avg_bytes_per_sec != exp_avg_bytes_per_sec) {
        log_error("WAV: invalid header (4).");
        error_r = EINVAL;
      }
      if (error_r == 0
          && header->block_align != channels_count * bits_per_sample / 8) {
        log_error("WAV: invalid header (5).");
        error_r = EINVAL;
      }

      result->channels_count = channels_count;
      result->samples_per_sec = samples_per_sec;
      result->bytes_per_sample = bits_per_sample / 8;
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
      log_error("WAV: invalid header (6).");
      error_r = EINVAL;
    }
    if (error_r == 0) {
      size_t frame_size = pcm_spec_get_frame_size(result);
      if (header->data_size % frame_size != 0) {
        log_error("WAV: invalid header (6)");
        error_r = EINVAL;
      } else {
        if (header->data_size % frame_size != 0) {
          log_error("WAV: not aligned data size: %d", header->data_size);
          error_r = EINVAL;
        } else {
          result->frames_count = header->data_size / frame_size;
        }
      }
    }
    return error_r;
  }

static error_t
pcm_validate_wav_content(
  struct io_rf_stream *stream,
  struct pcm_spec *result) {
    size_t fmt_length;
    error_t error_r = validate_wav_header(stream, result, &fmt_length);
    if (error_r == 0) {
      error_r = validate_wav_fmt_chunk_header(stream, fmt_length, result);
    }
    if (error_r == 0) {
      error_r = validate_wav_data_chunk_header(stream, result);
    }
    if (error_r == 0) {
      result->is_signed = result->bytes_per_sample > 1;
      pcm_spec_log("WAV", result);
    }
    return error_r;
  }

struct pcm_decoder_wav {
  struct pcm_decoder base;
};

static error_t
pcm_decoder_wav_decode_once(struct pcm_decoder *handler) {
  const struct io_buffer* src = pcm_decoder_get_source_buffer(handler);
  assert(io_buffer_get_unread_size(src) > 0);
  assert(!pcm_decoder_is_output_buffer_full(handler));

  void* data;
  size_t count = io_rf_stream_read_array(
    handler->src, 1, &data, io_buffer_get_available_size(&handler->dest));
  assert(io_buffer_try_write(&handler->dest, count, data));
  return 0;
}

static void
pcm_decoder_wav_release(struct pcm_decoder *handler) {
  assert(handler != NULL);
  struct pcm_decoder_wav *to_release = (struct pcm_decoder_wav*) handler;

  io_rf_stream_release(&to_release->base.src);
  io_buffer_free(&to_release->base.dest);
}

error_t
pcm_decoder_wav_open(
  struct io_rf_stream *src,
  struct pcm_decoder **decoder) {
    log_verbose("Setting up PCM decoder for WAV");
    assert(!io_rf_stream_is_empty(src));
    struct pcm_decoder_wav *result = calloc(1, sizeof(struct pcm_decoder_wav));
    error_t error_r = 0;
    if (result == NULL) {
      log_error("WAV: Insufficient memory for 'pcm_decoder_wav'");
      error_r = ENOMEM;
    }
    if (error_r == 0) {
      error_r = pcm_validate_wav_content(src, &result->base.spec);
    }
    if (error_r == 0) {
      result->base.src = src;
      result->base.block_size = pcm_spec_get_frame_size(&result->base.spec);
      result->base.decode_once = &pcm_decoder_wav_decode_once;
      result->base.release = &pcm_decoder_wav_release;
      *decoder = (struct pcm_decoder*)result;
    } else if (result != NULL) {
      pcm_decoder_wav_release((struct pcm_decoder*)result);
      free(result);
    }
    return error_r;
  }
