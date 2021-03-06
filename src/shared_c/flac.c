#include <errno.h>
#include <FLAC/stream_decoder.h>
#include <stdlib.h>
#include <string.h>
#include "flac.h"

struct pcm_decoder_flac {
  struct pcm_decoder base;
  FLAC__StreamDecoder *flac_decoder;
};

static void
metadata_callback(
  const FLAC__StreamDecoder *flac_decoder,
  const FLAC__StreamMetadata *metadata,
  void *client_data) {
    UNUSED(flac_decoder);
    assert(metadata != NULL);
    assert(client_data != NULL);
    struct pcm_decoder_flac *decoder = (struct pcm_decoder_flac*)client_data;
    struct pcm_spec *spec = &decoder->base.spec;

    if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
      const FLAC__StreamMetadata_StreamInfo *info = &metadata->data.stream_info;
      spec->bits_per_sample = info->bits_per_sample;
      spec->channels_count = info->channels;
      spec->samples_per_sec = info->sample_rate;
      spec->samples_count = info->total_samples;
      spec->is_big_endian = false;
      spec->is_signed = spec->bits_per_sample > 8;
      decoder->base.block_size = info->max_blocksize * pcm_frame_size(spec);
    }
  }

static FLAC__StreamDecoderReadStatus
read_callback(
    const FLAC__StreamDecoder *flac_decoder,
    FLAC__byte buffer[],
    size_t *bytes,
    void *client_data) {
      UNUSED(flac_decoder);
      assert(buffer != NULL);
      assert(bytes != NULL);
      assert(client_data != NULL);
      struct pcm_decoder_flac *decoder = (struct pcm_decoder_flac*)client_data;
      struct io_rf_stream *src = decoder->base.src;

      void* data;
      size_t read_count = io_rf_stream_read_array(
        src, sizeof(FLAC__byte), &data, *bytes);

      if (read_count == 0 && *bytes > 0) {
        // this is likely reading of metadata
        error_t error_r = EAGAIN;
        while (error_r == EAGAIN) {
          error_r = io_rf_stream_read_with_poll(src, -1);
        }
        if (error_r == 0) {
          read_count = io_rf_stream_read_array(
            src, sizeof(FLAC__byte), &data, *bytes);
        } else {
          log_error("FLAC: cannot read file in read_callback");
          return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
        }
      }

      if (read_count == 0) {
        // there is nothing to read in source stream
        if (io_rf_stream_is_eof(src)) {
          *bytes = 0;
          return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
        } else {
          log_error("FLAC: cannot read from the stream");
          return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
        }
      } else {
        *bytes = read_count;
        memcpy(buffer, data, read_count);
        return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
      }
    }

static FLAC__StreamDecoderWriteStatus
write_callback(
    const FLAC__StreamDecoder *flac_decoder,
    const FLAC__Frame *frame,
    const FLAC__int32 *const buffer[],
    void *client_data) {
      UNUSED(flac_decoder);
      assert(frame != NULL);
      assert(buffer != NULL);
      assert(client_data != NULL);
      struct pcm_decoder_flac *decoder = (struct pcm_decoder_flac*)client_data;
      const struct pcm_spec *spec = &decoder->base.spec;
      struct io_buffer *dest = &decoder->base.dest;

      const FLAC__FrameHeader* header_info = &frame->header;
      assert(header_info != NULL);
      if (header_info->bits_per_sample != spec->bits_per_sample) {
        log_error(
          "FLAC: expected bps %d, got %d",
          spec->bits_per_sample,
          header_info->bits_per_sample);
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
      }
      if (header_info->channels != spec->channels_count) {
        log_error(
          "FLAC: expected channels count %d, got %d",
          spec->channels_count,
          header_info->channels);
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
      }
      if (header_info->sample_rate != spec->samples_per_sec) {
        log_error(
          "FLAC: expected sample rate %d, got %d",
          spec->samples_per_sec,
          header_info->sample_rate);
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
      }

      size_t sample_size = header_info->bits_per_sample / 8;
      for (uint32_t i = 0; i < header_info->blocksize; i++) {
        for (uint32_t c = 0; c < header_info->channels; c++) {
          FLAC__int32 sample = buffer[0][i];
          bool result = io_buffer_try_write(dest, sample_size, &sample);
          if (!result) {
            log_error("FLAC: cannot write samples");
            return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
          }
        }
      }
      return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
    }

static void
error_callback(
  const FLAC__StreamDecoder *flac_decoder,
  FLAC__StreamDecoderErrorStatus status,
  void *client_data) {
    UNUSED(flac_decoder);
    UNUSED(client_data);
    log_error(
      "FLAC: decoding error: %s",
      FLAC__StreamDecoderErrorStatusString[status]);
  }

#define LOG_SETUP_ERROR(f, m)  if (!f) log_error(m)

static error_t
pcm_validate_flac_content(struct pcm_decoder_flac *decoder) {
  FLAC__StreamDecoder *flac_decoder = FLAC__stream_decoder_new();
  if (flac_decoder == NULL) {
    log_error("FLAC: decoder allocation failed");
    return EINVAL;
  }

  decoder->flac_decoder = flac_decoder;
  LOG_SETUP_ERROR(
    FLAC__stream_decoder_set_md5_checking(flac_decoder, true),
    "FLAC: Setting up md5 checking failed");

  FLAC__StreamDecoderInitStatus status = FLAC__stream_decoder_init_stream(
    flac_decoder,
    read_callback,
    NULL, NULL, NULL, NULL,
    write_callback, metadata_callback, error_callback,
    (void*)decoder); // NOLINT

  if (status  != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
    log_error(
      "FLAC: stream init: %s",
      FLAC__StreamDecoderInitStatusString[status]);
      return EINVAL;
  }

  if (!FLAC__stream_decoder_process_until_end_of_metadata(flac_decoder)) {
    FLAC__StreamDecoderState state = FLAC__stream_decoder_get_state(
      flac_decoder);
    log_error(
      "FLAC: read metadata: %s",
      FLAC__StreamDecoderStateString[state]);
    return EINVAL;
  }

  pcm_spec_log("FLAC", &decoder->base.spec);
  return 0;
}

static error_t
pcm_decoder_flac_decode_once(struct pcm_decoder *handler) {
  assert(handler != NULL);
  assert(io_rf_stream_get_unread_buffer_size(handler->src) > 0);
  assert(!io_buffer_is_full(&handler->dest));

  struct pcm_decoder_flac *decoder = (struct pcm_decoder_flac*)handler;
  if (!FLAC__stream_decoder_process_single(decoder->flac_decoder)) {
    FLAC__StreamDecoderState state = FLAC__stream_decoder_get_state(
      decoder->flac_decoder);
    if (state == FLAC__STREAM_DECODER_END_OF_STREAM) {
      assert(io_rf_stream_is_eof(handler->src));
      if (!FLAC__stream_decoder_finish(decoder->flac_decoder)) {
        log_error("FLAC: there is an issue with MD5 signature");
      }
    } else {
      log_error(
        "FLAC: single decoding: %s",
        FLAC__StreamDecoderStateString[state]);
      return EINVAL;
    }
  }
  return 0;
}

static void
pcm_decoder_flac_release(struct pcm_decoder **handler) {
  assert(handler != NULL);
  struct pcm_decoder_flac *to_release = (struct pcm_decoder_flac*) *handler;
  if (to_release != NULL) {
    io_buffer_free(&to_release->base.dest);
    FLAC__stream_decoder_delete(to_release->flac_decoder);
    free(to_release);
    *handler = NULL;
  }
}

error_t
pcm_decoder_flac_open(
  struct io_rf_stream *src,
  size_t buffer_size,
  struct pcm_decoder **decoder) {
    log_verbose("Setting up PCM decoder for FLAC");
    assert(!io_rf_stream_is_empty(src));
    struct pcm_decoder_flac *result =
      (struct pcm_decoder_flac*)calloc(1, sizeof(struct pcm_decoder_flac));
    error_t error_r = 0;
    if (result == NULL) {
      log_error("FLAC: Insufficient memory for 'pcm_decoder_flac'");
      error_r = ENOMEM;
    }
    if (error_r == 0) {
      result->base.src = src;
      error_r = pcm_validate_flac_content(result);
    }
    if (error_r == 0) {
      if (buffer_size < result->base.block_size) {
        log_verbose(
          "FLAC: increasing buffer to %d due to block size",
          result->base.block_size);
        buffer_size = result->base.block_size;
      }
      error_r = io_buffer_alloc(buffer_size, &result->base.dest);
    }
    if (error_r == 0) {
      result->base.decode_once = &pcm_decoder_flac_decode_once;
      result->base.release = &pcm_decoder_flac_release;
      *decoder = (struct pcm_decoder*)result;
    } else {
      pcm_decoder_flac_release((struct pcm_decoder**)&result);
    }
    return error_r;
  }
