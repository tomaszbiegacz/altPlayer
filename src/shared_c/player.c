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
  snd_pcm_uframes_t written_frames;
};

static error_t
get_pcm_format(const struct pcm_spec *spec, snd_pcm_format_t *format) {
  switch (spec->bits_per_sample) {
  case 8:
    *format = spec->is_signed ? SND_PCM_FORMAT_S8 : SND_PCM_FORMAT_U8;
    return 0;
  case 16:
    if (spec->is_big_endian)
      *format = spec->is_signed ? SND_PCM_FORMAT_S16_BE : SND_PCM_FORMAT_U16_BE;
    else
      *format = spec->is_signed ? SND_PCM_FORMAT_S16_LE : SND_PCM_FORMAT_U16_LE;
    return 0;
  case 24:
    if (spec->is_big_endian)
      *format = spec->is_signed ?
        SND_PCM_FORMAT_S24_3BE : SND_PCM_FORMAT_U24_3BE;
    else
      *format = spec->is_signed ?
        SND_PCM_FORMAT_S24_3LE : SND_PCM_FORMAT_U24_3LE;
    return 0;
  case 32:
    if (spec->is_big_endian)
      *format = spec->is_signed ? SND_PCM_FORMAT_S32_BE : SND_PCM_FORMAT_U32_BE;
    else
      *format = spec->is_signed ? SND_PCM_FORMAT_S32_LE : SND_PCM_FORMAT_U32_LE;
    return 0;
  }

  log_error("PLAYER: Unsupported player bitrate: %d", spec->bits_per_sample);
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

    log_verbose(
      "PLAYER: Resamplng is %s",
      params->disable_resampling ? "OFF" : "ON");
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_set_rate_resample(
        player_handle, hw_params, params->disable_resampling ? 0 : 1),
      "PLAYER: Resampling setup failed for playback: %s");

    log_verbose(
      "PLAYER: Stream parameters are %uHz, %s, %u channels",
      stream_spec->samples_per_sec,
      snd_pcm_format_name(pcm_format),
      stream_spec->channels_count);
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_set_access(
        player_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED),
      "PLAYER: Access type not available for playback: %s");
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_set_format(player_handle, hw_params, pcm_format),
      "PLAYER: Sample format not available for playback: %s");
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_set_channels(
        player_handle, hw_params, stream_spec->channels_count),
      "PLAYER: Channels count not available for playbacks: %s");

    int dir = 0;
    unsigned int samples_per_sec = stream_spec->samples_per_sec;
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_set_rate_near(
        player_handle, hw_params, &samples_per_sec, &dir),
      "PLAYER: Rate not available for playback: %s");
    if (samples_per_sec != stream_spec->samples_per_sec) {
      log_error(
        "PLAYER: Rate doesn't match (requested %uHz, get %uHz)",
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
        "PLAYER: Unable to get max period size for playback: %s");
      dir = 0;
      RETURN_ON_SNDERROR(
        snd_pcm_hw_params_get_period_size_min(
          hw_params, &period_size_min, &dir),
        "PLAYER: Unable to get min period size for playback: %s");

      size_t requested = *period_buffer_size / pcm_frame_size(stream_spec);
      log_verbose(
        "PLAYER: Frames per period (min, max, requested): %d, %d, %d (%d%)",
        period_size_min, period_size_max,
        requested, requested * 100 / period_size_max);
    }

    unsigned int period_time = pcm_buffer_time_us(
      stream_spec, *period_buffer_size);
    dir = 0;
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_set_period_time_near(
        player_handle, hw_params, &period_time, &dir),
      "PLAYER: Unable to set period time for playback: %s");
    if (period_time < 100000) {
      log_error(
        "PLAYER: Period time is smaller than 100ms: %dus",
        period_time);
    }

    dir = 0;
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_get_period_size(hw_params, frames_per_period, &dir),
      "PLAYER: Unable to get period size for playback: %s");
    size_t period_size = *frames_per_period * pcm_frame_size(stream_spec);
    if (period_size > *period_buffer_size) {
      log_error(
        "PLAYER: Period size %d is greater than period buffer size %d",
        period_size, *period_buffer_size);
    }
    *period_buffer_size = period_size;

    log_verbose(
      "PLAYER: Period time %dms (%d frames, %dkb)",
      period_time / 1000,
      *frames_per_period,
      period_size / 1024);

    unsigned int reads_per_period = max_uint(3, params->reads_per_period);
    *read_timeout = max_int(1, period_time / 1000 / reads_per_period);
    log_verbose("PLAYER: Read timeout: %d", *read_timeout);

    return 0;
  }

static error_t
player_set_params_buffer(
  snd_pcm_t *player_handle,
  snd_pcm_hw_params_t *hw_params,
  const struct player_parameters *params,
  const struct pcm_spec *stream_spec,
  size_t frames_per_period) {
    int dir;
    error_t error_r;
    size_t period_size = frames_per_period * pcm_frame_size(stream_spec);

    if (log_is_verbose()) {
      snd_pcm_uframes_t buffer_size_max, buffer_size_min;
      RETURN_ON_SNDERROR(
        snd_pcm_hw_params_get_buffer_size_max(
          hw_params, &buffer_size_max),
        "PLAYER: Unable to get max buffer size for playback: %s");
      RETURN_ON_SNDERROR(
        snd_pcm_hw_params_get_buffer_size_min(
          hw_params, &buffer_size_min),
        "PLAYER: Unable to get min buffer size for playback: %s");

      size_t requested = frames_per_period * params->periods_per_buffer;
      log_verbose(
        "PLAYER: Frames per buffer (min, max, requested): %d, %d, %d (%d%)",
        buffer_size_min, buffer_size_max,
        requested, requested * 100 / buffer_size_max);
    }

    snd_pcm_uframes_t frames_per_buffer;
    unsigned int buffer_time = pcm_buffer_time_us(
      stream_spec, period_size * max_int(params->periods_per_buffer, 16));
    dir = 0;
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_set_buffer_time_near(
        player_handle, hw_params, &buffer_time, &dir),
      "PLAYER: Unable to set buffer time for playback: %s");
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_get_buffer_size(hw_params, &frames_per_buffer),
      "PLAYER: Unable to get buffer size for playback: %s");
    log_verbose(
      "PLAYER: Buffer time %dms (%d periods, %d frames, %dkB)",
      buffer_time / 1000,
      frames_per_buffer / frames_per_period,
      frames_per_buffer,
      frames_per_buffer * pcm_frame_size(stream_spec) / 1024);
    return 0;
  }

static error_t
player_set_params_sw(
  snd_pcm_t *player_handle,
  snd_pcm_uframes_t frames_per_period) {
    error_t error_r = 0;
    snd_pcm_sw_params_t *sw_params = NULL;

    snd_pcm_sw_params_alloca(&sw_params);
    if (sw_params == NULL) {
      log_error("PLAYER: cannot allocate sw_params");
      return ENOMEM;
    }

    RETURN_ON_SNDERROR(
      snd_pcm_sw_params_current(
        player_handle, sw_params),
      "PLAYER: Unable to get software parameters: %s");

    RETURN_ON_SNDERROR(
      snd_pcm_sw_params_set_start_threshold(
        player_handle, sw_params, frames_per_period),
      "PLAYER: Unable to set start threshold: %s");

    RETURN_ON_SNDERROR(
      snd_pcm_sw_params(player_handle, sw_params),
      "PLAYER: Unable to set sw params for playback: %s");

    return error_r;
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
    if (hw_params == NULL) {
      log_error("PLAYER: cannot allocate hw_params");
      return ENOMEM;
    }
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_any(player_handle, hw_params),
      "PLAYER: no configurations available: %s");

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
        *frames_per_period);
    }

    RETURN_ON_SNDERROR(
      snd_pcm_hw_params(player_handle, hw_params),
      "PLAYER: Unable to set hw params for playback: %s");

    return player_set_params_sw(player_handle, *frames_per_period);
  }

static error_t
preload_first_period(struct player *player) {
  const size_t expected = player->frames_per_period;

  // part of first read has been used on metadata, do full read
  error_t error_r = pcm_decoder_read_source(player->decoder, -1);
  while (
    error_r == 0
    && pcm_decoder_is_source_buffer_ready_to_read(player->decoder)
    && pcm_decoder_get_output_buffer_frames_count(player->decoder) < expected) {
      error_r = pcm_decoder_decode_once(player->decoder);
      if (error_r == 0
        && !pcm_decoder_is_source_buffer_empty(player->decoder)) {
          error_r = pcm_decoder_read_source(player->decoder, -1);
        }
    }

  return error_r;
}

error_t
player_open(
  const struct player_parameters *params,
  struct pcm_decoder *pcm_stream,
  struct player **player) {
    log_verbose("Setting up ALSA player");
    assert(pcm_stream != NULL);
    assert(player != NULL);
    error_t error_r;
    snd_output_t *output = NULL;
    snd_pcm_t *player_handle = NULL;

    const char *device_name = params->device_name != NULL ?
      params->device_name : "default";

    log_verbose("PLAYER: ALSA library version:  %s", SND_LIB_VERSION_STR);
    log_verbose("PLAYER: Playback device: [%s]", device_name);
    RETURN_ON_SNDERROR(
      snd_output_stdio_attach(&output, stdout, 0),
      "PLAYER: Attaching output failed: %s");
    RETURN_ON_SNDERROR(
      snd_pcm_open(
        &player_handle,
        device_name,
        SND_PCM_STREAM_PLAYBACK,
        SND_PCM_NONBLOCK),
      "PLAYER: Playback open error: %s");

    struct player *result = (struct player*)calloc(1, sizeof(struct player));
    if (result == NULL) {
      log_error("PLAYER: Cannot allocate memory for player");
      snd_pcm_close(player_handle);
      error_r = ENOMEM;
    } else {
      result->handle = player_handle;
    }

    if (error_r == 0) {
      size_t period_size = params->period_size;
      if (period_size == 0) {
        period_size = max_size_t(
          64 * pcm_frame_size(&pcm_stream->spec),  // ALSA min
          io_buffer_get_allocated_size(&pcm_stream->dest));
      }
      error_r = player_set_params(
        result->handle, params,
        &pcm_stream->spec, period_size,
        &result->frames_per_period, &result->blocking_read_timeout);
    }

    if (error_r == 0) {
      result->written_frames = 0;
      result->decoder = pcm_stream;
      *player = result;
      if (log_is_verbose()) {
        snd_pcm_dump(result->handle, output);
      }
      error_r = preload_first_period(result);
    }
    if (error_r != 0) {
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
    if (error_r == -EPIPE) {
      log_verbose("PLAYER: recovery due to broken pipe");
      RETURN_ON_SNDERROR(
        snd_pcm_prepare(player_handle),
        "PLAYER: Can't recovery from underrun, prepare failed: %s");
      return 0;
    } else if (error_r == -ESTRPIPE) {
      log_verbose("PLAYER: recovery due to stream pipe error");
      error_t resume_error_r = snd_pcm_resume(player_handle);
      while (resume_error_r == -EAGAIN) {
        log_verbose("PLAYER: recovery sleep");
        sleep(1);
        resume_error_r = snd_pcm_resume(player_handle);
      }
      if (resume_error_r < 0) {
        RETURN_ON_SNDERROR(
          snd_pcm_prepare(player_handle),
          "PLAYER: Can't recovery from suspend, prepare failed: %s");
      }
      return 0;
    } else {
      log_verbose("PLAYER: can't recovery player from %d", error_r);
      return error_r;
    }
}

bool
player_is_eof(struct player *player) {
  assert(player != NULL);
  bool is_source_empty = pcm_decoder_is_source_buffer_empty(player->decoder);
  bool is_output_empty = pcm_decoder_is_output_buffer_empty(player->decoder);
  snd_pcm_state_t playback_state = snd_pcm_state(player->handle);
  return is_source_empty
    && is_output_empty
    && playback_state == SND_PCM_STATE_XRUN;
}

static error_t
player_preload(struct player *player, bool *has_been_waiting) {
  error_t error_r = 0;
  if (pcm_decoder_is_source_empty(player->decoder)) {
    // nothing to read from the source, we are done here
    *has_been_waiting = false;
  } else {
    if (pcm_decoder_is_source_buffer_full(player->decoder)) {
      // source buffer is full, but we can still decode it
      *has_been_waiting = false;
    } else {
      // fill up the buffer if possible with give read timeout
      // if source is empty hope for the best
      int poll_timeout = pcm_decoder_is_source_buffer_empty(player->decoder) ?
        -1 : player->blocking_read_timeout;
      error_r = pcm_decoder_read_source(player->decoder, poll_timeout);
    }

    while (
      error_r == 0
      && pcm_decoder_is_source_buffer_ready_to_read(player->decoder)
      && !pcm_decoder_is_output_buffer_full(player->decoder)) {
        error_r = pcm_decoder_decode_once(player->decoder);
      }
  }

  return error_r;
}

static error_t
player_write_alsa(struct player *player) {
  snd_pcm_sframes_t avail = snd_pcm_avail(player->handle);
  if (avail == 0) {
    // there is nothing to do here
    return 0;
  }

  error_t error_r = 0;
  size_t frame_size = pcm_decoder_frame_size(player->decoder);
  struct io_buffer *buffer = &player->decoder->dest;
  void* pcm;
  size_t count;
  io_buffer_array_items(buffer, frame_size, &pcm, &count);
  assert(count > 0);

  size_t avail_count = min_size_t(avail, count);
  int write_result = snd_pcm_writei(player->handle, pcm, avail_count);
  if (write_result == -EAGAIN) {
    // let's try again
    error_r = player_write_alsa(player);
  } else {
    if (write_result < 0) {
      error_t err_recovery = xrun_recovery(player->handle, write_result);
      if (err_recovery < 0) {
        error_r = write_result;
        log_error("PLAYER: Write error: %s", snd_strerror(error_r));
      }
    } else {
      io_buffer_array_seek(buffer, frame_size, write_result);
      player->written_frames += write_result;
    }
  }

  return error_r;
}

error_t
player_process_once(struct player *player) {
  assert(player != NULL);
  bool has_been_waiting;
  error_t error_r = player_preload(player, &has_been_waiting);
  if (error_r == 0 && !pcm_decoder_is_output_buffer_empty(player->decoder)) {
    error_r = player_write_alsa(player);
  }
  if (error_r == 0 && !has_been_waiting) {
    usleep(1000 * player->blocking_read_timeout);
  }

  return error_r;
}

error_t
player_get_playback_status(
  struct player *player,
  struct player_playback_status *result) {
    assert(player != NULL);
    assert(result != NULL);

    result->total = pcm_decoder_get_total_time(player->decoder);
    result->stream_buffer = pcm_decoder_get_source_buffer_unread_size(
      player->decoder);

    snd_pcm_sframes_t delay;
    error_t error_r = 0;
    RETURN_ON_SNDERROR(
      snd_pcm_delay(player->handle, &delay),
      "PLAYER: getting delay: %s");

    snd_pcm_uframes_t current = player->written_frames - delay;
    result->actual = pcm_spec_get_samples_time(
      &player->decoder->spec, current);
    result->playback_buffer = pcm_spec_get_samples_time(
      &player->decoder->spec, delay);
    return 0;
  }
