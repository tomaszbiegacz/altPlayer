#include <alsa/asoundlib.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "./log.h"
#include "./player.h"

struct player_settings {
  const char *device_name;
  bool allow_resampling;
  unsigned int channels_count;
  unsigned int stream_rate;
  unsigned int buffer_time_us;
  unsigned int period_time_us;
  snd_pcm_format_t sample_format;
  snd_pcm_access_t memory_access;
};

inline static size_t
frame_physical_width(const struct player_settings* params) {
  return params->channels_count
    * snd_pcm_format_physical_width(params->sample_format) / 8;
}

#define RETURN_ON_SNDERROR(f, e)  err = f;\
  if (err < 0) {\
    log_error(e, snd_strerror(err));\
    return err;\
  }

static int
set_player_params(
  snd_pcm_t *handle,
  const struct player_settings* params,
  snd_pcm_uframes_t* period_size
) {
    int err;
    int dir = 0;
    snd_pcm_hw_params_t *hw_params = NULL;

    snd_pcm_hw_params_alloca(&hw_params);
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_any(handle, hw_params),
      "Broken configuration for playback: no configurations available: %s");

    log_verbose("Resamplng is %s", params->allow_resampling ? "ON" : "OFF");
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_set_rate_resample(
        handle, hw_params, params->allow_resampling ? 1 : 0),
      "Resampling setup failed for playback: %s");

    log_verbose(
      "Stream parameters are %uHz, %s, %u channels",
      params->stream_rate,
      snd_pcm_format_name(params->sample_format),
      params->channels_count);
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_set_access(handle, hw_params, params->memory_access),
      "Access type not available for playback: %s");
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_set_format(handle, hw_params, params->sample_format),
      "Sample format not available for playback: %s");
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_set_channels(handle, hw_params, params->channels_count),
      "Channels count not available for playbacks: %s");

    unsigned int rrate = params->stream_rate;
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_set_rate_near(handle, hw_params, &rrate, &dir),
      "Rate not available for playback: %s");
    if (rrate != params->stream_rate) {
      log_error(
        "Rate doesn't match (requested %uHz, get %uHz)",
        params->stream_rate, rrate);
      return EINVAL;
    }

    snd_pcm_uframes_t buffer_size;
    unsigned buffer_time = params->buffer_time_us;
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_set_buffer_time_near(
        handle, hw_params, &buffer_time, &dir),
      "Unable to set buffer time for playback: %s");
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_get_buffer_size(hw_params, &buffer_size),
      "Unable to get buffer size for playback: %s");
    size_t buffer_size_physical = buffer_size * frame_physical_width(params);
    log_verbose(
      "Buffer time %d ms (%d frames, %d kB)",
      buffer_time / 1000,
      buffer_size,
      buffer_size_physical / 1024);

    unsigned period_time = params->period_time_us;
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_set_period_time_near(
        handle, hw_params, &period_time, &dir),
      "Unable to set period time for playback: %s");
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_get_period_size(hw_params, period_size, &dir),
      "Unable to get period size for playback: %s");
    log_verbose(
      "Period time %d ms (%d frames)",
      period_time / 1000,
      *period_size);

    RETURN_ON_SNDERROR(
      snd_pcm_hw_params(handle, hw_params),
      "Unable to set hw params for playback: %s");

    return err;
}

static int
setup_player(
  snd_pcm_t **player_handle,
  const struct player_settings *params,
  snd_pcm_uframes_t* period_size
) {
  int err;
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

  snd_pcm_t* handle = *player_handle;
  err = set_player_params(handle, params, period_size);
  if (err == 0 && log_is_verbose()) {
    snd_pcm_dump(handle, output);
    log_verbose(
      "PCM state: %s",
      snd_pcm_state_name(snd_pcm_state(handle)));
  }

  return err;
}

static int
get_format_from_signed_bitrate(unsigned int bitrate, snd_pcm_format_t* format) {
  switch (bitrate) {
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

  log_error("Unsupported player bitrate: %d", bitrate);
  return EINVAL;
}

static int
xrun_recovery(snd_pcm_t *handle, int err) {
    log_verbose("stream recovery");
    if (err == -EPIPE) {
        RETURN_ON_SNDERROR(
          snd_pcm_prepare(handle),
          "Can't recovery from underrun, prepare failed: %s")
    } else if (err == -ESTRPIPE) {
        while ((err = snd_pcm_resume(handle)) == -EAGAIN)
            // wait until suspend flag is released
            sleep(1);
        if (err < 0) {
            RETURN_ON_SNDERROR(
              snd_pcm_prepare(handle),
              "Can't recovery from suspend, prepare failed: %s")
        }
        return 0;
    }
    return err;
}

static int
write_loop(
  snd_pcm_t *handle,
  const struct io_memory_block* pcm,
  size_t frame_width,
  snd_pcm_uframes_t period_size
) {
  const char* ptr = pcm->data;
  size_t remaining_width = pcm->size;
  while (remaining_width > 0) {
    snd_pcm_uframes_t current_period_size = period_size;
    if (current_period_size * frame_width < remaining_width) {
      current_period_size = remaining_width / frame_width;
    }
    if (current_period_size == 0) {
      log_error("Last frame is incomplete?");
      remaining_width = 0;
    }

    while (current_period_size > 0) {
      int err = snd_pcm_writei(handle, ptr, current_period_size);
      if (err == -EAGAIN)
        continue;

      if (err < 0) {
        int err_recovery = xrun_recovery(handle, err);
        if (err_recovery < 0) {
          log_error("Write error: %s", snd_strerror(err));
          return err;
        }
      } else {
        const size_t transfered_width = err * frame_width;
        ptr += transfered_width;
        current_period_size -= err;
        remaining_width -= transfered_width;
      }
    }
  }
  return 0;
}

int
player_play_wav_pcm(const struct wav_pcm_content* wav) {
  int err;
  snd_pcm_format_t format;
  snd_pcm_t *handle = NULL;

  struct io_memory_block buffer = { 0 };
  snd_pcm_uframes_t period_size;
  size_t frame_width;

  err = get_format_from_signed_bitrate(wav->bits_per_sample, &format);
  if (err == 0) {
    struct player_settings params = (struct player_settings) {
      .device_name = "default",
      .allow_resampling = true,
      .channels_count = wav->channels_count,
      .stream_rate = wav->samples_per_sec,
      .buffer_time_us = 5000000,
      .period_time_us = 100000,
      .sample_format = format,
      .memory_access = SND_PCM_ACCESS_RW_INTERLEAVED
    };
    err = setup_player(&handle, &params, &period_size);
    frame_width = frame_physical_width(&params);
  }

  err = write_loop(handle, &(wav->pcm), frame_width, period_size);
  if (err == 0) {
    log_info("all OK");
  }

  snd_pcm_drain(handle);
  snd_pcm_close(handle);
  io_free_memory_block(&buffer);

  return err;
}
