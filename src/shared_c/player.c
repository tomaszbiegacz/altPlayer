#include <alsa/asoundlib.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "log.h"
#include "player.h"

#define RETURN_ON_SNDERROR(f, e)  error_r = f;\
  if (error_r < 0) {\
    log_error(e, snd_strerror(error_r));\
    return error_r;\
  }

struct player {
  struct pcm_decoder *decoder;
  snd_pcm_uframes_t frames_per_period;
  int blocking_read_timeout;
  snd_pcm_t *handle;
};

static error_t
get_pcm_format(
  const struct pcm_spec *stream_spec,
  snd_pcm_format_t *format) {
    switch (stream_spec->bits_per_sample) {
    case 8:
      *format = SND_PCM_FORMAT_S8;
      return 0;
    case 16:
      *format = SND_PCM_FORMAT_S16_LE;
      return 0;
    case 24:
      *format = SND_PCM_FORMAT_S24_3LE;
      return 0;
    case 32:
      *format = SND_PCM_FORMAT_S32_LE;
      return 0;
    }

    log_error("Unsupported player bitrate: %d", stream_spec->bits_per_sample);
    return EINVAL;
  }

static error_t
player_set_params_stream(
  snd_pcm_t *player_handle,
  snd_pcm_hw_params_t *hw_params,
  const struct player_parameters *params,
  const struct pcm_spec *stream_spec) {
    snd_pcm_format_t pcm_format;
    error_t error_r = get_pcm_format(stream_spec, &pcm_format);
    if (error_r != 0) {
      return error_r;
    }

    log_verbose("Resamplng is %s", params->disable_resampling ? "OFF" : "ON");
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_set_rate_resample(
        player_handle, hw_params, params->disable_resampling ? 0 : 1),
      "Resampling setup failed for playback: %s");

    log_verbose(
      "Stream parameters are %uHz, %s, %u channels",
      stream_spec->samples_per_sec,
      snd_pcm_format_name(pcm_format),
      stream_spec->channels_count);
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_set_access(
        player_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED),
      "Access type not available for playback: %s");
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_set_format(player_handle, hw_params, pcm_format),
      "Sample format not available for playback: %s");
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_set_channels(
        player_handle, hw_params, stream_spec->channels_count),
      "Channels count not available for playbacks: %s");

    int dir = 0;
    unsigned int samples_per_sec = stream_spec->samples_per_sec;
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_set_rate_near(
        player_handle, hw_params, &samples_per_sec, &dir),
      "Rate not available for playback: %s");
    if (samples_per_sec != stream_spec->samples_per_sec) {
      log_error(
        "Rate doesn't match (requested %uHz, get %uHz)",
        stream_spec->samples_per_sec, samples_per_sec);
      return EINVAL;
    }

    return error_r;
  }

static error_t
player_set_params_period(
  snd_pcm_t *player_handle,
  snd_pcm_hw_params_t *hw_params,
  const struct player_parameters *params,
  const struct pcm_spec *stream_spec,
  size_t *period_buffer_size,
  snd_pcm_uframes_t *frames_per_period,
  int *read_timeout) {
    int dir;
    error_t error_r;

    if (log_is_verbose()) {
      snd_pcm_uframes_t period_size_max, period_size_min;
      dir = 0;
      RETURN_ON_SNDERROR(
        snd_pcm_hw_params_get_period_size_max(
          hw_params, &period_size_max, &dir),
        "Unable to get max period size for playback: %s");
      dir = 0;
      RETURN_ON_SNDERROR(
        snd_pcm_hw_params_get_period_size_min(
          hw_params, &period_size_min, &dir),
        "Unable to get min period size for playback: %s");

      log_verbose(
        "Frames per period (min, max, requested): %d, %d, %d",
        period_size_min, period_size_max,
        *period_buffer_size / pcm_frame_size(stream_spec));
    }

    unsigned int period_time = pcm_buffer_time_us(
      stream_spec, *period_buffer_size);
    dir = 0;
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_set_period_time_near(
        player_handle, hw_params, &period_time, &dir),
      "Unable to set period time for playback: %s");
    if (period_time < 100000) {
      log_error("Period time is smaller than 100ms: %dus", period_time);
    }

    dir = 0;
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_get_period_size(hw_params, frames_per_period, &dir),
      "Unable to get period size for playback: %s");
    size_t period_size = *frames_per_period * pcm_frame_size(stream_spec);
    if (period_size > *period_buffer_size) {
      log_error(
        "Period size %d is greater than period buffer size %d",
        period_size, *period_buffer_size);
    }
    *period_buffer_size = period_size;

    log_verbose(
      "Period time %dms (%d frames, %dkb)",
      period_time / 1000,
      *frames_per_period,
      period_size / 1024);

    unsigned int reads_per_period = max_uint(3, params->reads_per_period);
    *read_timeout = max_int(1, period_time / 1000 / reads_per_period);
    log_verbose("Read timeout: %d", *read_timeout);

    return 0;
  }

static error_t
player_set_params_buffer(
  snd_pcm_t *player_handle,
  snd_pcm_hw_params_t *hw_params,
  const struct player_parameters *params,
  const struct pcm_spec *stream_spec,
  size_t period_size) {
    int dir;
    error_t error_r;

    if (log_is_verbose()) {
      snd_pcm_uframes_t buffer_size_max, buffer_size_min;
      RETURN_ON_SNDERROR(
        snd_pcm_hw_params_get_buffer_size_max(
          hw_params, &buffer_size_max),
        "Unable to get max buffer size for playback: %s");
      RETURN_ON_SNDERROR(
        snd_pcm_hw_params_get_buffer_size_min(
          hw_params, &buffer_size_min),
        "Unable to get min buffer size for playback: %s");

      log_verbose(
        "Frames per buffer (min, max, requested): %d, %d, %d",
        buffer_size_min, buffer_size_max,
        period_size * params->periods_per_buffer / pcm_frame_size(stream_spec));
    }

    snd_pcm_uframes_t frames_per_buffer;
    unsigned int buffer_time = pcm_buffer_time_us(
      stream_spec, period_size * max_int(params->periods_per_buffer, 16));
    dir = 0;
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_set_buffer_time_near(
        player_handle, hw_params, &buffer_time, &dir),
      "Unable to set buffer time for playback: %s");
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_get_buffer_size(hw_params, &frames_per_buffer),
      "Unable to get buffer size for playback: %s");
    log_verbose(
      "Buffer time %dms (%d frames, %dkB)",
      buffer_time / 1000,
      frames_per_buffer,
      frames_per_buffer * pcm_frame_size(stream_spec) / 1024);
    return 0;
  }

static error_t
player_set_params(
  snd_pcm_t *player_handle,
  const struct player_parameters *params,
  const struct pcm_spec *stream_spec,
  size_t period_buffer_size,
  snd_pcm_uframes_t *frames_per_period,
  int *read_timeout) {
    error_t error_r = 0;
    snd_pcm_hw_params_t *hw_params = NULL;

    snd_pcm_hw_params_alloca(&hw_params);
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_any(player_handle, hw_params),
      "Broken configuration for playback: no configurations available: %s");

    error_r = player_set_params_stream(
      player_handle, hw_params, params, stream_spec);
    if (error_r == 0) {
      error_r = player_set_params_period(
        player_handle, hw_params, params, stream_spec,
        &period_buffer_size, frames_per_period, read_timeout);
    }
    if (error_r == 0) {
      error_r = player_set_params_buffer(
        player_handle, hw_params, params, stream_spec,
        period_buffer_size);
    }
    if (error_r == 0) {
      RETURN_ON_SNDERROR(
        snd_pcm_hw_params(player_handle, hw_params),
        "Unable to set hw params for playback: %s");
    }

    return error_r;
  }

error_t
player_open(
  const struct player_parameters *params,
  struct pcm_decoder *pcm_stream,
  struct player **player) {
    assert(pcm_stream != NULL);
    assert(player != NULL);
    error_t error_r;
    snd_output_t *output = NULL;
    snd_pcm_t *player_handle = NULL;

    const char *device_name = params->device_name != NULL ?
      params->device_name : "default";

    log_verbose("ALSA library version:  %s", SND_LIB_VERSION_STR);
    log_verbose("Playback device: [%s]", device_name);
    RETURN_ON_SNDERROR(
      snd_output_stdio_attach(&output, stdout, 0),
      "Attaching output failed: %s");
    RETURN_ON_SNDERROR(
      snd_pcm_open(
        &player_handle,
        device_name,
        SND_PCM_STREAM_PLAYBACK,
        0),
      "Playback open error: %s");

    struct player *result = (struct player*)calloc(1, sizeof(struct player));
    if (result == NULL) {
      log_error("Cannot allocate memory for player");
      snd_pcm_close(player_handle);
      error_r = ENOMEM;
    } else {
      result->handle = player_handle;
    }

    if (error_r == 0) {
      error_r = player_set_params(
        result->handle, params,
        &pcm_stream->spec, io_buffer_get_allocated_size(&pcm_stream->dest),
        &result->frames_per_period, &result->blocking_read_timeout);
    }

    if (error_r == 0) {
      result->decoder = pcm_stream;
      *player = result;
      if (log_is_verbose()) {
        snd_pcm_dump(result->handle, output);
      }
    } else {
      player_release(&result);
    }
    return error_r;
  }

void
player_release(struct player **player) {
  assert(player != NULL);
  struct player *to_release = *player;
  if (to_release != NULL) {
    snd_pcm_drain(to_release->handle);
    snd_pcm_close(to_release->handle);
  }
  *player = NULL;
}

static error_t
xrun_recovery(snd_pcm_t *player_handle, error_t error_r) {
    log_verbose("stream recovery");
    if (error_r == -EPIPE) {
        RETURN_ON_SNDERROR(
          snd_pcm_prepare(player_handle),
          "Can't recovery from underrun, prepare failed: %s")
    } else if (error_r == -ESTRPIPE) {
        while ((error_r = snd_pcm_resume(player_handle)) == -EAGAIN)
            // wait until suspend flag is released
            sleep(1);
        if (error_r < 0) {
            RETURN_ON_SNDERROR(
              snd_pcm_prepare(player_handle),
              "Can't recovery from suspend, prepare failed: %s")
        }
        return 0;
    }
    return error_r;
}

bool
player_is_eof(struct player *player) {
  assert(player != NULL);
  return pcm_decoder_is_source_buffer_empty(player->decoder)
    &&  pcm_decoder_is_output_buffer_empty(player->decoder);
}

error_t
player_process_once(struct player *player) {
  assert(player != NULL);
  size_t frame_size = pcm_decoder_frame_size(player->decoder);

  int poll_timeout = pcm_decoder_is_source_buffer_empty(player->decoder) ?
    -1 : 0;
  error_t error_r = pcm_decoder_read_source(player->decoder, poll_timeout);

  while (
    error_r == 0
    && !pcm_decoder_is_source_buffer_empty(player->decoder)
    && !pcm_decoder_is_output_buffer_full(player->decoder)) {
      error_r = pcm_decoder_decode_once(player->decoder);
    }

  if (error_r == 0 && !pcm_decoder_is_output_buffer_empty(player->decoder)) {
    struct io_buffer *buffer = &player->decoder->dest;
    void* pcm;
    size_t count;
    io_buffer_array_items(buffer, frame_size, &pcm, &count);
    assert(count > 0);

    int write_result = snd_pcm_writei(player->handle, pcm, count);
    if (write_result == -EAGAIN) {
      error_r = EAGAIN;
    } else {
      if (write_result < 0) {
        error_t err_recovery = xrun_recovery(player->handle, write_result);
        if (err_recovery < 0) {
          error_r = write_result;
          log_error("Write error: %s", snd_strerror(error_r));
        }
      } else {
        io_buffer_array_seek(buffer, frame_size, write_result);
      }
    }
  }

  return error_r;
}
