#include <alsa/asoundlib.h>
#include <stdio.h>
#include "log.h"
#include "alsa_player.h"
#include "timer.h"

#define RETURN_ON_SNDERROR(f, e)  error_r = f;\
  if (error_r < 0) {\
    log_error(e, snd_strerror(error_r));\
    return error_r;\
  }

struct alsa_player {
  snd_pcm_t *handle;
  snd_output_t *alsa_output;
  int blocking_read_timeout;
  struct pcm_decoder *decoder;
  snd_pcm_format_t sample_format;
  snd_pcm_uframes_t frames_per_period;
  snd_pcm_uframes_t written_frames;
  bool is_fillingup_io_buffer;
  bool is_writing_silence;
};

//
// Open player
//

static error_t
player_set_params_hw_session(
  snd_pcm_t *player_handle,
  const struct alsa_player_parameters *player_spec) {
    error_t error_r;

    snd_pcm_hw_params_t *hw_params = NULL;
    snd_pcm_hw_params_alloca(&hw_params);
    if (hw_params == NULL) {
      log_error("PLAYER: cannot allocate hw_params");
      return ENOMEM;
    }
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_any(player_handle, hw_params),
      "PLAYER: unable to get hw parameters: %s");

    log_verbose(
      "PLAYER: Resamplng is %s",
      player_spec->disable_resampling ? "OFF" : "ON");
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_set_rate_resample(
        player_handle, hw_params, player_spec->disable_resampling ? 0 : 1),
      "PLAYER: Resampling setup failed for playback: %s");

    if (error_r == 0) {
      RETURN_ON_SNDERROR(
        snd_pcm_hw_params(player_handle, hw_params),
        "PLAYER: Unable to set hw params for player: %s");
    }

    return error_r;
  }

static void
alsa_log_setup(const struct alsa_player *player) {
  if (player->alsa_output != NULL) {
    snd_pcm_dump(player->handle, player->alsa_output);
  }
}

error_t
alsa_player_open(
  const struct alsa_player_parameters *params,
  struct alsa_player **result) {
    assert(params != NULL);
    assert(result != NULL);
    error_t error_r;

    snd_output_t *alsa_output = NULL;
    if (log_is_verbose()) {
      RETURN_ON_SNDERROR(
        snd_output_stdio_attach(&alsa_output, stdout, 0),
        "PLAYER: Attaching output failed: %s");
    }

    const char *device_name = params->hardware_id != NULL ?
      params->hardware_id : "default";
    log_verbose("PLAYER: ALSA library version:  %s", SND_LIB_VERSION_STR);
    log_verbose("PLAYER: Playback device: [%s]", device_name);

    snd_pcm_t *player_handle = NULL;
    RETURN_ON_SNDERROR(
      snd_pcm_open(
        &player_handle,
        device_name,
        SND_PCM_STREAM_PLAYBACK,
        SND_PCM_NONBLOCK),
      "PLAYER: Playback open error: %s");

    struct alsa_player *player =
      (struct alsa_player*)calloc(1, sizeof(struct alsa_player));
    if (result == NULL) {
      log_error("PLAYER: Cannot allocate memory for player");
      snd_output_close(alsa_output);
      snd_pcm_close(player_handle);
      error_r = ENOMEM;
    } else {
      player->handle = player_handle;
      player->alsa_output = alsa_output;
    }
    if (error_r == 0) {
      error_r = player_set_params_hw_session(player_handle, params);
    }
    if (error_r == 0) {
      *result = player;
      alsa_log_setup(player);
    } else if (player != NULL) {
      alsa_player_release(&player);
    }
    return error_r;
  }

void
alsa_player_release(struct alsa_player **player_ref) {
  assert(player_ref != NULL);
  struct alsa_player *player = *player_ref;
  if (player != NULL) {
    pcm_decoder_release(&player->decoder);

    if (player->alsa_output != NULL) {
      snd_output_close(player->alsa_output);
    }
    snd_pcm_close(player->handle);
  }
  *player_ref = NULL;
}

//
// Open stream
//

static error_t
get_pcm_format(const struct pcm_spec *spec, snd_pcm_format_t *format) {
  u_int16_t bites_per_sample = pcm_spec_get_bits_per_sample(spec);
  bool is_signed = pcm_spec_is_signed(spec);
  bool is_big_endian = pcm_spec_is_big_endian(spec);

  switch (bites_per_sample) {
  case 8:
    *format = is_signed ? SND_PCM_FORMAT_S8 : SND_PCM_FORMAT_U8;
    return 0;
  case 16:
    if (is_big_endian)
      *format = is_signed ? SND_PCM_FORMAT_S16_BE : SND_PCM_FORMAT_U16_BE;
    else
      *format = is_signed ? SND_PCM_FORMAT_S16_LE : SND_PCM_FORMAT_U16_LE;
    return 0;
  case 24:
    if (is_big_endian)
      *format = is_signed ?
        SND_PCM_FORMAT_S24_3BE : SND_PCM_FORMAT_U24_3BE;
    else
      *format = is_signed ?
        SND_PCM_FORMAT_S24_3LE : SND_PCM_FORMAT_U24_3LE;
    return 0;
  case 32:
    if (is_big_endian)
      *format = is_signed ? SND_PCM_FORMAT_S32_BE : SND_PCM_FORMAT_U32_BE;
    else
      *format = is_signed ? SND_PCM_FORMAT_S32_LE : SND_PCM_FORMAT_U32_LE;
    return 0;
  }

  log_error("PLAYER: Unsupported player bitrate: %d", bites_per_sample);
  return EINVAL;
}

static error_t
player_get_period_size(
  snd_pcm_hw_params_t *hw_params,
  int blocking_read_timeout,
  const struct pcm_spec *pcm_spec,
  snd_pcm_uframes_t *frames_in_period) {
    int dir;
    error_t error_r;
    snd_pcm_uframes_t period_size_max, period_size_min;

    dir = 0;
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_get_period_size_min(
        hw_params, &period_size_min, &dir),
      "PLAYER: Unable to get min period size for playback: %s");

    dir = 0;
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_get_period_size_max(
        hw_params, &period_size_max, &dir),
      "PLAYER: Unable to get max period size for playback: %s");

    snd_pcm_uframes_t total_frames = pcm_spec_get_frames_count(pcm_spec);
    snd_pcm_uframes_t wait_frames = pcm_spec_get_frames_count_for_time(
      pcm_spec,
      max_int(blocking_read_timeout, 100));

    *frames_in_period = min_size_t(
      period_size_max,
      max_size_t(period_size_min, min_size_t(total_frames, wait_frames)));

    return 0;
  }

static error_t
player_get_buffer_size(
  snd_pcm_hw_params_t *hw_params,
  snd_pcm_uframes_t frames_per_period,
  snd_pcm_uframes_t *frames_per_buffer) {
    error_t error_r;
    snd_pcm_uframes_t buffer_size_max;

    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_get_buffer_size_max(
        hw_params, &buffer_size_max),
      "PLAYER: Unable to get max buffer size for playback: %s");

    *frames_per_buffer = buffer_size_max - buffer_size_max % frames_per_period;
    return 0;
}

static error_t
player_set_period_size(
  struct alsa_player* player,
  snd_pcm_hw_params_t *hw_params,
  const struct pcm_spec *pcm_spec) {
    int dir;
    error_t error_r = player_get_period_size(
      hw_params,
      player->blocking_read_timeout,
      pcm_spec,
      &player->frames_per_period);
    if (error_r != 0) {
      return error_r;
    }

    snd_pcm_t *player_handle = player->handle;
    dir = 0;
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_set_period_size(
        player_handle, hw_params, player->frames_per_period, dir),
      "PLAYER: Unable to set period size for playback: %s");

    dir = 0;
    unsigned int period_time_us;
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_get_period_time(hw_params, &period_time_us, &dir),
      "PLAYER: Unable to get period size for playback: %s");

    log_verbose(
      "PLAYER: Period time %dms (%d frames, %dkb)",
      period_time_us / 1000,
      player->frames_per_period,
      player->frames_per_period * pcm_spec_get_frame_size(pcm_spec) / 1024);

    return 0;
  }

static error_t
player_set_buffer_size(
  struct alsa_player* player,
  snd_pcm_hw_params_t *hw_params,
  const struct pcm_spec *pcm_spec) {
    error_t error_r;
    int dir;

    snd_pcm_uframes_t frames_per_buffer;
    error_r = player_get_buffer_size(
      hw_params, player->frames_per_period, &frames_per_buffer);
    if (error_r != 0) {
      return error_r;
    }

    snd_pcm_t *player_handle = player->handle;
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_set_buffer_size(
        player_handle, hw_params, frames_per_buffer),
      "PLAYER: Unable to set buffer size for playback: %s");

    dir = 0;
    unsigned int buffer_time;
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params_get_buffer_time(hw_params, &buffer_time, &dir),
      "PLAYER: Unable to get buffer time for playback: %s");
    log_verbose(
      "PLAYER: Buffer time %dms (%d periods, %d frames, %dkB)",
      buffer_time / 1000,
      frames_per_buffer / player->frames_per_period,
      frames_per_buffer,
      frames_per_buffer * pcm_spec_get_frame_size(pcm_spec) / 1024);

    return error_r;
  }

static error_t
player_set_params_hw_stream(struct alsa_player* player) {
  snd_pcm_t *player_handle = player->handle;
  const struct pcm_spec *pcm_spec = pcm_decoder_get_spec(player->decoder);

  snd_pcm_format_t pcm_format;
  error_t error_r = get_pcm_format(pcm_spec, &pcm_format);
  if (error_r != 0) {
    return error_r;
  }

  snd_pcm_hw_params_t *hw_params = NULL;
  snd_pcm_hw_params_alloca(&hw_params);
  if (hw_params == NULL) {
    log_error("PLAYER: cannot allocate hw_params");
    return ENOMEM;
  }
  RETURN_ON_SNDERROR(
    snd_pcm_hw_params_any(player_handle, hw_params),
    "PLAYER: unable to get hw parameters: %s");

  log_verbose(
    "PLAYER: Stream parameters are %uHz, %s, %u channels",
    pcm_spec_get_samples_per_sec(pcm_spec),
    snd_pcm_format_name(pcm_format),
    pcm_spec_get_channels_count(pcm_spec));
  RETURN_ON_SNDERROR(
    snd_pcm_hw_params_set_access(
      player_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED),
    "PLAYER: Access type not available for playback: %s");
  RETURN_ON_SNDERROR(
    snd_pcm_hw_params_set_format(player_handle, hw_params, pcm_format),
    "PLAYER: Sample format not available for playback: %s");
  RETURN_ON_SNDERROR(
    snd_pcm_hw_params_set_channels(
      player_handle, hw_params, pcm_spec->channels_count),
    "PLAYER: Channels count not available for playbacks: %s");
  player->sample_format = pcm_format;

  int dir = 0;
  unsigned int samples_per_sec = pcm_spec_get_samples_per_sec(pcm_spec);
  RETURN_ON_SNDERROR(
    snd_pcm_hw_params_set_rate_near(
      player_handle, hw_params, &samples_per_sec, &dir),
    "PLAYER: Rate not available for playback: %s");
  if (samples_per_sec != pcm_spec_get_samples_per_sec(pcm_spec)) {
    log_error(
      "PLAYER: Rate doesn't match (requested %uHz, get %uHz)",
      pcm_spec_get_samples_per_sec(pcm_spec),
      samples_per_sec);
    return EINVAL;
  }

  error_r = player_set_period_size(player, hw_params, pcm_spec);
  if (error_r == 0) {
    error_r = player_set_buffer_size(player, hw_params, pcm_spec);
  }

  if (error_r == 0) {
    RETURN_ON_SNDERROR(
      snd_pcm_hw_params(player_handle, hw_params),
      "PLAYER: Unable to set hw params for stream: %s");
  }

  return error_r;
}

static error_t
player_set_params_sw_stream(struct alsa_player* player) {
  snd_pcm_t *player_handle = player->handle;
  error_t error_r;

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
      player_handle, sw_params, player->frames_per_period),
    "PLAYER: Unable to set start threshold: %s");

  RETURN_ON_SNDERROR(
    snd_pcm_sw_params(player_handle, sw_params),
    "PLAYER: Unable to set sw params for playback: %s");

  return error_r;
}

static error_t
read_source_once(
  struct alsa_player *player,
  bool *has_been_waiting) {
    error_t error_r = 0;
    if (pcm_decoder_can_source_buffer_fillup(player->decoder)) {
      bool is_ready  = pcm_decoder_is_source_ready_to_fillup(player->decoder);
      if (player->is_fillingup_io_buffer || is_ready) {
          *has_been_waiting = true;
          player->is_fillingup_io_buffer = true;
          error_r = pcm_decoder_read_source_try(
            player->decoder,
            player->blocking_read_timeout);
        } else {
          player->is_fillingup_io_buffer = false;
        }
    } else {
      player->is_fillingup_io_buffer = false;
    }
    return error_r;
  }

static error_t
decode_to_output(struct alsa_player *player) {
  error_t error_r = 0;

  const struct io_buffer* src;
  src = pcm_decoder_get_source_buffer(player->decoder);
  while (
    error_r == 0
    && io_buffer_get_unread_size(src) > 0
    && !pcm_decoder_is_output_buffer_full(player->decoder)) {
      error_r = pcm_decoder_decode_once(player->decoder);
    }
  return error_r;
}

static error_t
read_decode_first_period(struct alsa_player *player) {
  int timeout = -1;  // there is always socket timeout
  const size_t expected = player->frames_per_period;

  error_t error_r = 0;
  const struct io_buffer* src = pcm_decoder_get_source_buffer(player->decoder);
  while (
    error_r == 0
    && pcm_decoder_can_source_buffer_fillup(player->decoder)
    && pcm_decoder_get_output_buffer_frames_count(player->decoder) < expected) {
      error_r = pcm_decoder_read_source_try(player->decoder, timeout);
      if (error_r == 0 && !io_buffer_is_empty(src)) {
        error_r = pcm_decoder_decode_once(player->decoder);
      }
    }

  if (error_r == 0) {
    size_t frames = pcm_decoder_get_output_buffer_frames_count(player->decoder);
    if (frames < expected) {
      log_verbose(
        "PLAYER: Expected to preload %d frames, got %d",
        expected,
        frames);
    }
  }

  return error_r;
}

error_t
alsa_player_start(
  struct alsa_player *player,
  struct pcm_decoder *pcm_stream) {
    assert(player != NULL);
    assert(pcm_stream != NULL);
    error_t error_r;

    error_r = alsa_player_stop(player);
    if (error_r == 0) {
      log_verbose("PLAYER: Starting stream");
      player->decoder = pcm_stream;
      player->written_frames = 0;
      player->is_fillingup_io_buffer = true;
      player->is_writing_silence = false;
      error_r = player_set_params_hw_stream(player);
    }
    if (error_r == 0) {
      error_r = pcm_decoder_allocate_output_buffer(
        pcm_stream,
        2 * player->frames_per_period);
    }
    if (error_r == 0) {
      error_r = player_set_params_sw_stream(player);
    }
    if (error_r == 0) {
      error_r = read_decode_first_period(player);
    }
    if (error_r == 0) {
      alsa_log_setup(player);
    } else {
      // don't release it here
      // maybe it will be possible to recover from the error
      player->decoder = NULL;
    }
    return error_r;
  }

error_t
alsa_player_stop(struct alsa_player *player) {
  assert(player != NULL);
  error_t error_r;

  snd_pcm_state_t playback_state = snd_pcm_state(player->handle);
  switch (playback_state) {
    case SND_PCM_STATE_RUNNING:
    case SND_PCM_STATE_XRUN:
    case SND_PCM_STATE_DRAINING:
    case SND_PCM_STATE_PAUSED:
      log_verbose(
        "PLAYER: Stopping from state %s",
        snd_pcm_state_name(playback_state));
      error_r = snd_pcm_drop(player->handle);
      if (error_r != 0) {
        log_error("PLAYER: Cannot stop PCM: %s", snd_strerror(error_r));
      }
      break;
    case SND_PCM_STATE_SUSPENDED:
    case SND_PCM_STATE_DISCONNECTED:
      log_error(
        "Cannot stop player in state: %s",
        snd_pcm_state_name(playback_state));
      // possibly try to reconnect HW
      error_r = ENOTRECOVERABLE;
      break;
    default:
      // all is OK
      error_r = 0;
      break;
  }

  if (error_r != 0) {
    pcm_decoder_release(&player->decoder);
  }
  return error_r;
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

static error_t
player_write_alsa(struct alsa_player *player) {
  snd_pcm_sframes_t avail = snd_pcm_avail(player->handle);
  if (avail == 0) {
    // there is nothing to do here
    return 0;
  }

  error_t error_r = 0;
  const struct pcm_spec *spec = pcm_decoder_get_spec(player->decoder);
  size_t frame_size = pcm_spec_get_frame_size(spec);
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

static void
fillup_output_with_silence(struct alsa_player* player) {
  if (!player->is_writing_silence) {
    log_verbose("PLAYER: Starting writing silence  ");
    player->is_writing_silence = true;
  }

  int32_t value = snd_pcm_format_silence_32(player->sample_format);
  while (!pcm_decoder_is_output_buffer_full(player->decoder)) {
    assert(pcm_decoder_try_write_sample(player->decoder, value));
  }
}

error_t
alsa_player_process_once(
  struct alsa_player *player,
  bool *has_been_waiting) {
    assert(player != NULL);
    assert(player->decoder != NULL);
    error_t error_r;

    if (!pcm_decoder_is_source_empty(player->decoder)) {
      error_r = read_source_once(player, has_been_waiting);
      if (error_r == 0) {
        error_r = decode_to_output(player);
      }
    } else {
      fillup_output_with_silence(player);
    }

    if (error_r == 0) {
      size_t frames;
      frames = pcm_decoder_get_output_buffer_frames_count(player->decoder);
      if (frames > 0) {
        error_r = player_write_alsa(player);
      }
    }
    return error_r;
  }

struct timespec
alsa_player_get_stream_time(const struct alsa_player *player) {
  assert(player != NULL);
  return pcm_spec_get_time(pcm_decoder_get_spec(player->decoder));
}

error_t
alsa_player_get_playback_status(
  const struct alsa_player *player,
  struct alsa_player_playback_status *result) {
    assert(player != NULL);
    assert(player->decoder != NULL);
    assert(result != NULL);

    result->stream_buffer = io_buffer_get_unread_size(
      pcm_decoder_get_source_buffer(player->decoder));

    error_t error_r = 0;
    snd_pcm_sframes_t delay = 0;
    if (player->written_frames > 0) {
      RETURN_ON_SNDERROR(
        snd_pcm_delay(player->handle, &delay),
        "PLAYER: getting delay: %s");
    }

    const struct pcm_spec* spec = pcm_decoder_get_spec(player->decoder);
    result->playback_buffer_frames = delay > 0 ? delay : 0;
    result->playback_buffer = pcm_spec_get_time_for_frames(
      spec,
      result->playback_buffer_frames);

    size_t stream_frames = pcm_spec_get_frames_count(spec);
    result->is_complete =
      player->written_frames - result->playback_buffer_frames >= stream_frames;

    if (result->is_complete) {
      result->position = alsa_player_get_stream_time(player);
    } else {
      result->position = pcm_spec_get_time_for_frames(
        spec,
        player->written_frames - result->playback_buffer_frames);
    }

    return error_r;
  }
