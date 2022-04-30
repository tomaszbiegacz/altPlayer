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

static inline size_t
get_min_wav_header_size() {
  return
    sizeof(struct wav_header)
    + sizeof(struct wav_fmt_chunk_header)
    + sizeof(struct wav_data_chunk_header);
}

struct event_wav_decoder {
  struct pcm_spec_builder *spec;
  enum event_wav_stage stage;
  size_t fmt_length_remaining;
};

static void
event_wav_decoder_free(void *arg) {
  struct event_wav_decoder *wav = (struct event_wav_decoder*)arg;
  pcm_spec_builder_release(&wav->spec);
}

struct event_wav_encoder {
  struct event_pcm *pcm;
  bool is_header_written;
};

static void
event_wav_encoder_free(void *arg) {
  UNUSED(arg);
  // intentionally empty
}

static error_t
read_wav_header(
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
read_fmt_chunk_header(
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
  struct event_wav_decoder *wav) {
    size_t fmt_length;
    error_t error_r = read_wav_header(input, wav->spec, &fmt_length);
    if (error_r == 0) {
      error_r = read_fmt_chunk_header(
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
  struct event_wav_decoder *wav) {
    const void *data;
    size_t read_count = cont_buf_read_array_begin(
      input, 1, &data, wav->fmt_length_remaining);
    if (read_count > 0) {
      cont_buf_read_array_commit(input, 1, read_count);
      wav->fmt_length_remaining -= read_count;
    }
  }

static error_t
read_data_chunk_header(
  struct cont_buf_read *input,
  struct event_wav_decoder *wav) {
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
pipe_on_read_decode(
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
    struct event_wav_decoder *wav = (struct event_wav_decoder*)arg; // NOLINT
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
          error_r = read_data_chunk_header(input, wav);
        }

    *is_input_end = wav->stage == event_wav_stage_pcm;
    return error_r;
  }

error_t
event_wav_decode(
  struct event_pipe *source,
  struct event_pcm **result_r) {
    struct event_wav_decoder *arg = NULL;
    error_t error_r = mem_calloc(
      "event_wav_decoder",
      sizeof(struct event_wav_decoder),
      (void**)&arg);  // NOLINT

    struct event_pcm *event = NULL;
    if (error_r == 0) {
      EMPTY_STRUCT(event_pipe_config, pipe_config);
      pipe_config.arg = arg;
      pipe_config.arg_free = event_wav_decoder_free;
      pipe_config.on_read = pipe_on_read_decode;

      error_r = mem_strdup("event_wav_decode", &pipe_config.name);
      if (error_r == 0) {
        error_r = event_pcm_create(source, &pipe_config, &event, &arg->spec);
      }
      if (error_r == 0) {
        error_r = event_pipe_set_read_lowmark(
          event_pcm_get_pipe(event),
          get_min_wav_header_size());
      }
      if (error_r == 0) {
        *result_r = event;
      } else {
        mem_free((void**)&pipe_config.name);  // NOLINT
        if (arg != NULL) {
          event_wav_decoder_free(arg);
        }
      }
    }
    return error_r;
  }

static void
cont_buf_write_assert(
  struct cont_buf *output,
  const void* data,
  size_t size) {
    size_t result = cont_buf_write(output, data, size);
    assert(result == size);
  }

static void
write_header(struct pcm_spec *spec, struct cont_buf *output) {
  EMPTY_STRUCT(wav_header, header);
  if (pcm_spec_is_big_endian(spec)) {
    memcpy(header.riff_marker, "RIFX", 4);
  } else {
    memcpy(header.riff_marker, "RIFF", 4);
  }
  memcpy(header.form_type, "WAVE", 4);
  memcpy(header.fmt_chunk_marker, "fmt ", 4);
  header.fmt_length = sizeof(struct wav_fmt_chunk_header);

  EMPTY_STRUCT(wav_fmt_chunk_header, fmt_chunk);
  fmt_chunk.fmt_format_type = 1;
  fmt_chunk.samples_per_sec = pcm_spec_get_samples_per_sec(spec);
  fmt_chunk.bits_per_sample = pcm_spec_get_bits_per_sample(spec);
  fmt_chunk.n_channels = pcm_spec_get_channels_count(spec);
  fmt_chunk.avg_bytes_per_sec =
    fmt_chunk.n_channels
    * fmt_chunk.samples_per_sec * fmt_chunk.bits_per_sample / 8;
  fmt_chunk.block_align = fmt_chunk.n_channels * fmt_chunk.bits_per_sample / 8;

  EMPTY_STRUCT(wav_data_chunk_header, data_chunk);
  memcpy(data_chunk.data_marker, "data", 4);
  data_chunk.data_size =
    pcm_spec_get_frames_count(spec) * pcm_spec_get_frame_size(spec);

  header.overall_size = data_chunk.data_size
    + get_min_wav_header_size()
    - 8;  // see http://tiny.systems/software/soundProgrammer/WavFormatDocs.pdf

  cont_buf_write_assert(output, (const void*)&header, sizeof(header));
  cont_buf_write_assert(output, (const void*)&fmt_chunk, sizeof(fmt_chunk));
  cont_buf_write_assert(output, (const void*)&data_chunk, sizeof(data_chunk));
}

static error_t
pipe_on_read_encode(
  struct event_pipe *pipe,
  void *arg,
  struct cont_buf_read *input,
  bool *is_input_end,
  struct cont_buf *output) {
    UNUSED(pipe);
    assert(arg != NULL);
    assert(input != NULL);
    assert(is_input_end != NULL);
    assert(output != NULL);

    error_t error_r = 0;
    struct event_wav_encoder *wav = (struct event_wav_encoder*)arg; // NOLINT
    if (!wav->is_header_written) {
      struct pcm_spec *spec = NULL;
      error_r = event_pcm_get_spec(wav->pcm, &spec);
      if (error_r == 0) {
        write_header(spec, output);
        wav->is_header_written = true;
      }
    }
    if (error_r == 0 && wav->is_header_written) {
      // this could be improved by changing pipe into intermediate
      if (cont_buf_read_is_empty(input)) {
        *is_input_end = true;
      } else {
        size_t move_count = cont_buf_move(output, input);
        assert(move_count > 0);
      }
    }
    return error_r;
  }

error_t
event_wav_encode(
  struct event_pcm *source,
  struct event_pipe **result_r) {
    struct event_wav_encoder *arg = NULL;
    struct cont_buf *buf = NULL;
    struct event_pipe *event = NULL;

    error_t error_r = mem_calloc(
      "event_wav_encoder",
      sizeof(struct event_wav_encoder),
      (void**)&arg);  // NOLINT
    if (error_r == 0) {
      error_r = cont_buf_create(1000, &buf);
    }
    if (error_r == 0) {
      arg->pcm = source;
      EMPTY_STRUCT(event_pipe_config, pipe_config);
      pipe_config.arg = arg;
      pipe_config.arg_free = event_wav_encoder_free;
      pipe_config.buffer = buf;
      pipe_config.on_read = pipe_on_read_encode;
      error_r = mem_strdup("event_wav_encode", &pipe_config.name);
      if (error_r == 0) {
        error_r = event_pipe_create(
          event_pcm_get_pipe(source),
          &pipe_config, &event);
      }
    }

    if (error_r == 0) {
      *result_r = event;
    } else {
      cont_buf_release(&buf);
    }

    return error_r;
  }
