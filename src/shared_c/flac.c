#include <errno.h>
#include "flac.h"

error_t
pcm_validate_flac_content(
    struct io_rf_stream *stream,
    int timeout,
    struct pcm_spec *result,
    struct io_stream_statistics *stats) {
  return EINVAL;
}