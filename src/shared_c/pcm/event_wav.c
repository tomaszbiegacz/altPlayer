#include "../log.h"
#include "../mem.h"
#include "event_wav.h"

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

enum event_wav_stage {
  event_wav_stage_ready       = 0,      // nothing has been read so far
  event_wav_stage_header_meta = 1,      // metadadata header parsing
  event_wav_stage_header_pcm  = 2,      // data header parsing
  event_wav_stage_pcm         = 3,      // pcm data
};

struct event_wav {
  struct pcm_spec_builder *spec;
  enum event_wav_stage stage;
  size_t fmt_length_remaining;
};

static void
event_wav_free(void *arg) {
  struct event_wav *wav = (struct event_wav*)arg;
  pcm_spec_builder_release(&wav->spec);
}

static inline size_t
get_min_header_size() {
  return
    sizeof(struct wav_header)
    + sizeof(struct wav_fmt_chunk_header)
    + sizeof(struct wav_data_chunk_header);
}

static error_t
validate_wav_header(
  struct cont_buf_read *input,
  struct pcm_spec_builder *spec,
  size_t *fmt_length) {
    const struct wav_header *header = NULL;
    error_t error_r = cont_buf_read_try(
      input,
      sizeof(struct wav_header),
      (const void**)&header) ? 0 : EINVAL; //NOLINT

    if (error_r == 0) {
      if (memcmp(header->riff_marker, "RIFF", 4) == 0) {
        // is_big_endian = false;
      } else if (memcmp(header->riff_marker, "RIFX", 4) == 0) {
        pcm_spec_set_big_endian(spec);
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
  struct cont_buf_read *input,
  struct pcm_spec_builder *spec,
  size_t fmt_length,
  size_t *remaining_fmt_length) {
    error_t error_r = 0;
    if (fmt_length < sizeof(struct wav_fmt_chunk_header)) {
      log_error(
        "WAV: memory allocated for fmt_header is too small: %d",
        fmt_length);
      error_r = EINVAL;
    }

    const struct wav_fmt_chunk_header *header = NULL;
    if (error_r == 0) {
      bool is_header_read = cont_buf_read_try(
        input,
        sizeof(struct wav_fmt_chunk_header),
        (const void**)&header); //NOLINT
      assert(is_header_read);
    }
    if (error_r == 0 && header->fmt_format_type != 1) {
      log_error("WAV: invalid header (3).");
      error_r = EINVAL;
    }
    if (error_r == 0) {
      const uint32_t samples_per_sec = header->samples_per_sec;
      const uint16_t bits_per_sample = header->bits_per_sample;
      const uint16_t channels_count = header->n_channels;

      const uint32_t exp_avg_bytes_per_sec =
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

      pcm_spec_set_channels_count(spec, channels_count);
      pcm_spec_set_samples_per_sec(spec, samples_per_sec);
      pcm_spec_set_bits_per_sample(spec, bits_per_sample);
      if (bits_per_sample > 8) {
        pcm_spec_set_signed(spec);
      }

      *remaining_fmt_length = fmt_length - sizeof(struct wav_fmt_chunk_header);
    }
    return error_r;
  }

static error_t
read_header_meta(
  struct cont_buf_read *input,
  struct event_wav *wav) {
    size_t fmt_length;
    error_t error_r = validate_wav_header(input, wav->spec, &fmt_length);
    if (error_r == 0) {
      error_r = validate_wav_fmt_chunk_header(
        input,
        wav->spec,
        fmt_length,
        &wav->fmt_length_remaining);
    }
    if (error_r == 0) {
      wav->stage = event_wav_stage_header_meta;
    }
    return error_r;
  }

static void
read_header_meta_complete(
  struct cont_buf_read *input,
  struct event_wav *wav) {
    const void *data;
    size_t read_count = cont_buf_read_array_begin(
      input, 1, &data, wav->fmt_length_remaining);
    if (read_count > 0) {
      cont_buf_read_array_commit(input, 1, read_count);
      wav->fmt_length_remaining -= read_count;
    }
  }

static error_t
validate_wav_data_chunk_header(
  struct cont_buf_read *input,
  struct event_wav *wav) {
    error_t error_r = 0;
    const struct wav_data_chunk_header *header;
    bool is_header_ready = cont_buf_read_try(
      input,
      sizeof(struct wav_data_chunk_header),
      (const void**)&header);  //NOLINT

    if (is_header_ready) {
      if (memcmp(header->data_marker, "data", 4) != 0) {
        log_error("WAV: invalid header (6).");
        error_r = EINVAL;
      }
      if (error_r == 0) {
        error_r = pcm_spec_set_data_size(wav->spec, header->data_size);
      }
      if (error_r == 0) {
        wav->stage = event_wav_stage_pcm;
      }
    }
    return error_r;
  }

static error_t
pipe_on_read(
  struct event_pipe *pipe,
  void *arg,
  struct cont_buf_read *input,
  bool *is_input_end,
  struct cont_buf *output) {
    UNUSED(pipe);
    assert(arg != NULL);
    assert(input != NULL);
    assert(is_input_end != NULL);
    assert(output == NULL);

    error_t error_r = 0;
    struct event_wav *wav = (struct event_wav*)arg; // NOLINT
    assert(wav->stage != event_wav_stage_pcm);

    if (wav->stage == event_wav_stage_ready) {
      error_r = read_header_meta(input, wav);
    }
    if (error_r == 0 && wav->fmt_length_remaining > 0) {
      assert(wav->stage == event_wav_stage_header_meta);
      read_header_meta_complete(input, wav);
    }
    if (error_r == 0
        && wav->stage == event_wav_stage_header_meta
        && wav->fmt_length_remaining == 0) {
          error_r = validate_wav_data_chunk_header(input, wav);
        }

    *is_input_end = wav->stage == event_wav_stage_pcm;
    return error_r;
  }

error_t
event_wav_decode(
  struct event_pipe *source,
  struct event_pcm **result_r) {
    struct event_wav *arg = NULL;
    error_t error_r = mem_calloc(
      "event_wav",
      sizeof(struct event_wav),
      (void**)&arg);  // NOLINT

    struct event_pcm *event = NULL;
    if (error_r == 0) {
      EMPTY_STRUCT(event_pipe_config, pipe_config);
      pipe_config.arg = arg;
      pipe_config.arg_free = event_wav_free;
      pipe_config.on_read = pipe_on_read;

      error_r = mem_strdup("event_wav", &pipe_config.name);
      if (error_r == 0) {
        error_r = event_pcm_create(source, &pipe_config, &event, &arg->spec);
      }
      if (error_r == 0) {
        error_r = event_pipe_set_read_lowmark(
          event_pcm_get_pipe(event),
          get_min_header_size());
      }
      if (error_r == 0) {
        *result_r = event;
      } else {
        mem_free((void**)&pipe_config.name);  // NOLINT
      }
    }
    return error_r;
  }
