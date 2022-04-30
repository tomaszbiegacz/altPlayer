#include <FLAC/stream_decoder.h>
#include "../mem.h"
#include "event_flac.h"

struct event_flac_decoder {
  struct event_pipe *pipe;
  FLAC__StreamDecoder *flac_decoder;

  struct pcm_spec_builder *spec;
  uint32_t max_blocksize;
  bool is_spec_ready;
  struct pcm_spec *spec_read;

  struct cont_buf_read *input_buffer;
  struct cont_buf *output_buffer;
};

static void
event_flac_decoder_free(void *arg) {
  struct event_flac_decoder *state = (struct event_flac_decoder*)arg;
  FLAC__stream_decoder_delete(state->flac_decoder);
  pcm_spec_builder_release(&state->spec);
  pcm_spec_release(&state->spec_read);
}

static void
on_error(
  const FLAC__StreamDecoder *flac_decoder,
  FLAC__StreamDecoderErrorStatus status,
  void *client_data) {
    UNUSED(flac_decoder);
    UNUSED(client_data);
    log_error(
      "FLAC: decoding error: %s",
      FLAC__StreamDecoderErrorStatusString[status]);
  }

static FLAC__StreamDecoderReadStatus
on_read_data(
    const FLAC__StreamDecoder *flac_decoder,
    FLAC__byte buffer[],
    size_t *bytes,
    void *arg) {
      UNUSED(flac_decoder);
      assert(buffer != NULL);
      assert(bytes != NULL);
      assert(*bytes > 0);
      assert(arg != NULL);
      struct event_flac_decoder *state = (struct event_flac_decoder*)arg;

      const void* read_data;
      size_t read_count = cont_buf_read_array_begin(
        state->input_buffer, sizeof(FLAC__byte), &read_data, *bytes);

      if (read_count == 0) {
        if (event_pipe_is_end(state->pipe)) {
          log_info("FLAC: closing the stream.");
          *bytes = 0;
          return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
        } else {
          log_error("FLAC: cannot read from the stream");
          return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
        }
      } else {
        *bytes = read_count;
        memcpy(buffer, read_data, read_count);
        cont_buf_read_array_commit(
          state->input_buffer,
          sizeof(FLAC__byte), read_count);
        return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
      }
    }

static void
on_read_metadata(
  const FLAC__StreamDecoder *flac_decoder,
  const FLAC__StreamMetadata *metadata,
  void *arg) {
    UNUSED(flac_decoder);
    assert(metadata != NULL);
    assert(arg != NULL);
    struct event_flac_decoder *decoder = (struct event_flac_decoder*)arg;

    if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
      assert(decoder->spec_read == NULL);
      const FLAC__StreamMetadata_StreamInfo *info = &metadata->data.stream_info;
      pcm_spec_set_bits_per_sample(decoder->spec, info->bits_per_sample);
      pcm_spec_set_channels_count(decoder->spec, info->channels);
      pcm_spec_set_samples_per_sec(decoder->spec, info->sample_rate);
      pcm_spec_set_frames_count(decoder->spec, info->total_samples);
      if (info->bits_per_sample > 8) {
        pcm_spec_set_signed(decoder->spec);
      }

      decoder->max_blocksize = info->max_blocksize;
      decoder->is_spec_ready = true;
    }
  }

static FLAC__StreamDecoderWriteStatus
on_read_pcm(
    const FLAC__StreamDecoder *flac_decoder,
    const FLAC__Frame *frame,
    const FLAC__int32 *const buffer[],
    void *arg) {
      UNUSED(flac_decoder);
      assert(frame != NULL);
      assert(buffer != NULL);
      assert(arg != NULL);
      struct event_flac_decoder *state = (struct event_flac_decoder*)arg;
      const struct pcm_spec *spec = state->spec_read;

      const FLAC__FrameHeader* header_info = &frame->header;
      assert(header_info != NULL);
      if (header_info->bits_per_sample != pcm_spec_get_bits_per_sample(spec)) {
        log_error(
          "FLAC: expected bps %d, got %d",
          pcm_spec_get_bits_per_sample(spec),
          header_info->bits_per_sample);
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
      }
      if (header_info->channels != pcm_spec_get_channels_count(spec)) {
        log_error(
          "FLAC: expected channels count %d, got %d",
          pcm_spec_get_channels_count(spec),
          header_info->channels);
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
      }
      if (header_info->sample_rate != pcm_spec_get_samples_per_sec(spec)) {
        log_error(
          "FLAC: expected sample rate %d, got %d",
          pcm_spec_get_samples_per_sec(spec),
          header_info->sample_rate);
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
      }

      size_t blocksize;
      void *data;
      uint16_t sample_size = pcm_spec_get_bytes_per_sample(spec);
      cont_buf_write_array_begin(
        state->output_buffer, sample_size, &data, &blocksize);
      assert(header_info->blocksize <= blocksize);

      char* output = data;
      for (uint32_t i = 0; i < header_info->blocksize; i++) {
        for (uint32_t c = 0; c < header_info->channels; c++) {
          FLAC__int32 sample = buffer[c][i];
          memcpy(output, &sample, sample_size);
          output += sample_size;
        }
      }
      cont_buf_write_array_commit(
        state->output_buffer,
        sample_size, header_info->blocksize);

      return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
    }

static error_t
flac_process_once(
  FLAC__StreamDecoder *flac_decoder,
  bool *is_end) {
    if (FLAC__stream_decoder_process_single(flac_decoder)) {
      FLAC__StreamDecoderState state = FLAC__stream_decoder_get_state(
        flac_decoder);
      if (state == FLAC__STREAM_DECODER_END_OF_STREAM) {
        *is_end = true;
        log_info("FLAC: closing stream");
        if (!FLAC__stream_decoder_finish(flac_decoder)) {
          log_error("FLAC: there is an issue with MD5 signature");
        }
        return 0;
      } else if (state > FLAC__STREAM_DECODER_END_OF_STREAM) {
        log_error(
          "FLAC: single decoding: %s",
          FLAC__StreamDecoderStateString[state]);
        return EINVAL;
      } else {
        // normal business
        return 0;
      }
    } else {
      log_error("FLAC: fatal error when decoding frame");
      return EINVAL;
    }
  }

inline static size_t
event_flac_decoder_req_write(const struct event_flac_decoder *state) {
  return state->max_blocksize * pcm_spec_get_frame_size(state->spec_read);
}

static error_t
pipe_read_metadata(struct event_flac_decoder *state) {
  error_t error_r = 0;
  assert(!cont_buf_read_is_empty(state->input_buffer));
  assert(!state->is_spec_ready);
  while (
    error_r == 0
    && !cont_buf_read_is_empty(state->input_buffer)
    && !state->is_spec_ready) {
      bool is_end = false;
      error_r = flac_process_once(state->flac_decoder, &is_end);
      if (error_r == 0 && is_end) {
        log_error("FLAC: end of stream while reading metadata");
        error_r = EINVAL;
      }
    }
  if (error_r == 0 && state->is_spec_ready) {
    error_r = pcm_spec_get(state->spec, &state->spec_read);
    if (error_r == 0) {
      error_r = event_pipe_set_write_lowmark(
        state->pipe,
        event_flac_decoder_req_write(state));
    }
  }
  return error_r;
}

static error_t
pipe_read_pcm(struct event_flac_decoder *state, bool *is_end) {
  error_t error_r = 0;
  size_t required_space = event_flac_decoder_req_write(state);
  if (cont_buf_read_is_empty(state->input_buffer)) {
    if(required_space <= cont_buf_get_available_size(state->output_buffer)) {
      error_r = flac_process_once(state->flac_decoder, is_end);
    }
  } else {
    while (
      error_r == 0
      && !cont_buf_read_is_empty(state->input_buffer)
      && required_space <= cont_buf_get_available_size(state->output_buffer)) {
        error_r = flac_process_once(state->flac_decoder, is_end);
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
    assert(output != NULL);

    struct event_flac_decoder *state = (struct event_flac_decoder*)arg; // NOLINT

    error_t error_r = 0;
    state->input_buffer = input;
    state->output_buffer = output;
    if (!state->is_spec_ready) {
      error_r = pipe_read_metadata(state);
    }
    if (error_r == 0 && state->is_spec_ready) {
      error_r = pipe_read_pcm(state, is_input_end);
    }
    return error_r;
  }

static error_t
init_flac_decoder(struct event_flac_decoder *state) {
  FLAC__StreamDecoder *flac_decoder = state->flac_decoder;
  error_t error_r = 0;
  if (!FLAC__stream_decoder_set_md5_checking(flac_decoder, true)) {
    log_error("FLAC: Setting up md5 checking failed");
    error_r = EINVAL;
  }
  if (error_r == 0) {
    FLAC__StreamDecoderInitStatus status = FLAC__stream_decoder_init_stream(
      flac_decoder,
      on_read_data,
      NULL, NULL, NULL, NULL,
      on_read_pcm, on_read_metadata, on_error,
      (void*)state); // NOLINT

    if (status  != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
      log_error(
        "FLAC: stream init: %s",
        FLAC__StreamDecoderInitStatusString[status]);
      error_r = EINVAL;
    }
  }
  return error_r;
}

error_t
event_flac_decode(
  struct event_pipe *source,
  struct event_pcm **result_r) {
    struct event_flac_decoder *arg = NULL;
    struct cont_buf *buf = NULL;
    struct event_pcm *event = NULL;

    error_t error_r = mem_calloc(
      "event_flac_decoder",
      sizeof(struct event_flac_decoder),
      (void**)&arg);  // NOLINT

    if (error_r == 0) {
      arg->flac_decoder = FLAC__stream_decoder_new();
      if (arg->flac_decoder == NULL) {
        log_error("FLAC: decoder allocation failed");
        error_r = EINVAL;
      }
    }
    if (error_r == 0) {
      error_r = init_flac_decoder(arg);
    }
    if (error_r == 0) {
      // this will be resized in pipe_read_metadata if needed
      error_r = cont_buf_create(2 * 4096, &buf);
    }
    if (error_r == 0) {
      EMPTY_STRUCT(event_pipe_config, pipe_config);
      pipe_config.arg = arg;
      pipe_config.arg_free = event_flac_decoder_free;
      pipe_config.buffer = buf;
      pipe_config.on_read = pipe_on_read_decode;

      error_r = mem_strdup("event_flac_decode", &pipe_config.name);
      if (error_r == 0) {
        error_r = event_pcm_create(source, &pipe_config, &event, &arg->spec);
      }
      if (error_r == 0) {
        error_r = event_pipe_set_read_lowmark(
          event_pcm_get_pipe(event),
          2 * 4096);
      }
      if (error_r == 0) {
        arg->pipe = event_pcm_get_pipe(event);
        *result_r = event;
      } else {
        mem_free((void**)&pipe_config.name);  // NOLINT
        if (arg != NULL) {
          event_flac_decoder_free(arg);
        }
      }
    }
    return error_r;
  }
