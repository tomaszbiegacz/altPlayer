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
player_set_params(
  snd_pcm_t *player_handle,
  const struct player_parameters *params,
  const struct pcm_spec *stream_spec,
  snd_pcm_uframes_t* frames_per_period
) {
    error_t error_r = 0;
    int dir = 0;
    snd_pcm_hw_params_t *hw_params = NULL;

    snd_pcm_format_t pcm_format;
    error_r = get_pcm_format(stream_spec, &pcm_format);
    if (error_r != 0) {
      return error_r;
    }

    snd_pcm_hw_params_alloca(&hw_params);
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_any(player_handle, hw_params),
      "Broken configuration for playback: no configurations available: %s");

    log_verbose("Resamplng is %s", params->allow_resampling ? "ON" : "OFF");
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_set_rate_resample(
        player_handle, hw_params, params->allow_resampling ? 1 : 0),
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

    unsigned int period_time = pcm_buffer_time_us(
      stream_spec, params->period_size);
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_set_period_time_near(
        player_handle, hw_params, &period_time, &dir),
      "Unable to set period time for playback: %s");
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_get_period_size(hw_params, frames_per_period, &dir),
      "Unable to get period size for playback: %s");
    size_t period_size = *frames_per_period * pcm_frame_size(stream_spec);
    log_verbose(
      "Period time %dms (%d frames, %dkb)",
      period_time / 1000,
      *frames_per_period,
      period_size / 1024);

    snd_pcm_uframes_t frames_per_buffer;
    unsigned int buffer_time = pcm_buffer_time_us(
      stream_spec, period_size * params->periods_per_buffer);
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

    RETURN_ON_SNDERROR(
      snd_pcm_hw_params(player_handle, hw_params),
      "Unable to set hw params for playback: %s");

    return error_r;
}

static error_t
player_setup(
  const struct player_parameters *params,
  const struct pcm_spec *stream_spec,
  snd_pcm_t **player_handle,
  snd_pcm_uframes_t* frames_per_period
) {
  error_t error_r;
  snd_output_t *output = NULL;

  log_verbose("ALSA library version:  %s", SND_LIB_VERSION_STR);
  log_verbose("Playback device: [%s]", params->device_name);

  RETURN_ON_SNDERROR(
    snd_output_stdio_attach(&output, stdout, 0),
    "Attaching output failed: %s");
  RETURN_ON_SNDERROR(
     snd_pcm_open(
       player_handle,
       params->device_name,
       SND_PCM_STREAM_PLAYBACK,
       0),
    "Playback open error: %s");

  error_r = player_set_params(
    *player_handle, params, stream_spec, frames_per_period);
  if (error_r == 0 && log_is_verbose()) {
    snd_pcm_dump(*player_handle, output);
  }

  return error_r;
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

static error_t
write_loop(
    snd_pcm_t *player_handle,
    int timeout,
    const struct pcm_spec *stream_spec,
    struct io_rf_stream *stream,
    snd_pcm_uframes_t frames_per_period,
    struct io_stream_statistics *stats) {
  const size_t frame_size = pcm_frame_size(stream_spec);
  const size_t full_period_size = frame_size * frames_per_period;
  error_t error_r = 0;
  size_t remaining_data_size = stream_spec->data_size;
  while (error_r == 0 && remaining_data_size > 0) {
    void *period;
    size_t period_size = mimum_size_t(remaining_data_size, full_period_size);
    error_r = io_rf_stream_seek_block(
      stream, timeout, period_size, &period, stats);

    snd_pcm_uframes_t period_frames = 0;
    if (error_r == 0) {
      remaining_data_size -= period_size;
      period_frames = period_size / frame_size;
      if (period_frames * frame_size !=  period_size) {
        log_error("Last frame is incomplete?");
      }
    }

    while (error_r == 0 && period_frames > 0) {
      int write_result = snd_pcm_writei(player_handle, period, period_frames);
      if (write_result != -EAGAIN)
      {
        if (write_result < 0) {
          error_t err_recovery = xrun_recovery(player_handle, write_result);
          if (err_recovery < 0) {
            error_r = write_result;
            log_error("Write error: %s", snd_strerror(error_r));
          }
        } else {
          period_frames -= write_result;
        }
      }
    }
  }
  return error_r;
}

error_t
player_pcm_play(
    const struct player_parameters *params,
    int timeout,
    const struct pcm_spec *stream_spec,
    struct io_rf_stream *stream,
    struct io_stream_statistics *stats) {
  assert(params != NULL);
  assert(stream_spec != NULL);
  assert(stream != NULL);

  snd_pcm_t *player_handle = NULL;
  snd_pcm_uframes_t frames_per_period;
  error_t error_r = player_setup(
    params, stream_spec, &player_handle, &frames_per_period);
  if (error_r == 0) {
    error_r = write_loop(
      player_handle, timeout, stream_spec, stream, frames_per_period, stats);
  }

  if (player_handle != NULL) {
    snd_pcm_drain(player_handle);
    snd_pcm_close(player_handle);
  }
  return error_r;
}
