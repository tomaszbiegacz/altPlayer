#include "pcm_decoder.h"

error_t
pcm_decoder_allocate_output_buffer(
  struct pcm_decoder *dec,
  size_t frames_count) {
    assert(dec != NULL);
    size_t buffer_size = pcm_spec_get_frame_size(&dec->spec) * frames_count;
    if (buffer_size < dec->block_size) {
      log_verbose(
        "FLAC: increasing buffer to %d due to block size",
        dec->block_size);
      buffer_size = dec->block_size;
    }
    return io_buffer_alloc(buffer_size, &dec->dest);
  }

error_t
pcm_decoder_read_source_try(struct pcm_decoder *dec, int poll_timeout) {
  const struct io_buffer *buffer = io_rf_stream_get_buffer(dec->src);
  if (!io_rf_stream_is_eof(dec->src) && !io_buffer_is_full(buffer)) {
    return io_rf_stream_read_with_poll(dec->src, poll_timeout);
  }
  return 0;
}

void
pcm_decoder_release(struct pcm_decoder **dec) {
  assert(dec != NULL);
  struct pcm_decoder *to_release = *dec;
  if (to_release != NULL) {
    to_release->release(to_release);
    free(to_release);
    *dec = NULL;
  }
}
