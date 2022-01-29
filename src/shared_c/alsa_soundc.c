#include <alsa/asoundlib.h>
#include <stdio.h>
#include "log.h"
#include "alsa_soundc.h"

#define RETURN_ON_SNDERROR(f, e)  error_r = f;\
  if (error_r < 0) {\
    log_error(e, snd_strerror(error_r));\
    return error_r;\
  }

struct sound_card_info {
  int card_index;
  char *hardware_id;
  char *long_name;
  char *driver_name;
  char *mixer_name;
  char *components;
};

static void
free_sound_card_info(struct sound_card_info *info) {
  if (info->hardware_id != NULL) {
    free(info->hardware_id);
    info->hardware_id = NULL;
  }
  if (info->long_name != NULL) {
    free(info->long_name);
    info->long_name = NULL;
  }
  if (info->driver_name != NULL) {
    free(info->driver_name);
    info->driver_name = NULL;
  }
  if (info->mixer_name != NULL) {
    free(info->mixer_name);
    info->mixer_name = NULL;
  }
  if (info->components != NULL) {
    free(info->components);
    info->components = NULL;
  }
}

static char*
get_sound_card_hardware_id(const char* card_id) {
  const char* prefix = "hw:CARD=";
  size_t result_size = strlen(prefix) + strlen(card_id) + 1;
  char *result = malloc(result_size);
  if (result != NULL) {
    strncpy(result, prefix, result_size);
    strncat(result, card_id, result_size);
  }
  return result;
}

static error_t
fetch_sound_card_details(snd_ctl_t *ctl, struct sound_card_info *info) {
  snd_ctl_card_info_t *card_info = NULL;
  snd_ctl_card_info_alloca(&card_info);
  if (card_info == NULL) {
    log_error("Cannot allocate memory for snd_ctl_card_info_t");
    return ENOMEM;
  }

  error_t error_r = snd_ctl_card_info(ctl, card_info);
  if (error_r != 0) {
    log_error(
      "PLAYER: Cannot get card sound card info for card [%d] due to %s",
      info->card_index,
      snd_strerror(error_r));
  } else {
    const char* card_id = snd_ctl_card_info_get_id(card_info);
    info->hardware_id = get_sound_card_hardware_id(card_id);
    info->long_name = strdup(snd_ctl_card_info_get_longname(card_info));
    info->driver_name = strdup(snd_ctl_card_info_get_driver(card_info));
    info->mixer_name = strdup(snd_ctl_card_info_get_mixername(card_info));
    info->components = strdup(snd_ctl_card_info_get_components(card_info));
  }
  return error_r;
}

static char*
get_sound_card_hardware_index(int card_index) {
  size_t result_size = 3 + 10 + 1;  // "hw:" + name + /0
  char *result = malloc(result_size);
  if (result != NULL) {
    snprintf(result, result_size, "hw:%d", card_index);
  }
  return result;
}

/**
 * @brief try to get any possible information and log occured errors
 *
 */
static error_t
open_and_fetch_sound_card_details(struct sound_card_info *info) {
  char* hardware_id = get_sound_card_hardware_index(info->card_index);
  if (hardware_id == 0) {
    log_error("PLAYER: Cannont allocate memory for hardware index");
    return ENOMEM;
  }

  snd_ctl_t *ctl = NULL;
  error_t  error_r = snd_ctl_open(&ctl, hardware_id, 0);
  if (error_r != 0) {
    log_error(
      "PLAYER: Cannot open sound card [%s] due to %s",
      hardware_id,
      snd_strerror(error_r));
  } else {
    error_r = fetch_sound_card_details(ctl, info);

    error_t error_close = snd_ctl_close(ctl);
    if (error_close != 0) {
      log_error(
        "PLAYER: Cannot close sound card [%s] due to %s",
        hardware_id,
        snd_strerror(error_close));

      if (error_r == 0) {
        error_r = error_close;
      }
    }
  }
  free(hardware_id);
  return error_r;
}

error_t
alsa_soundc_get_next_info(struct sound_card_info **info) {
  assert(info != NULL);
  if (*info == NULL) {
    *info = calloc(1, sizeof(struct sound_card_info));
    if (*info == NULL) {
      log_error("PLAYER: Cannot allocate memory for sound_card_info");
      return ENOMEM;
    }
    (*info)->card_index = -1;
  } else {
    free_sound_card_info(*info);
  }

  error_t error_r = 0;
  struct sound_card_info *current_info = *info;
  RETURN_ON_SNDERROR(
    snd_card_next(&current_info->card_index),
    "PLAYER: Getting next sound card id: %s");

  if (current_info->card_index == -1) {
    // no more sounds cards
    alsa_soundc_release(info);
  } else {
    error_r = open_and_fetch_sound_card_details(current_info);

    if (error_r != 0 && error_r != ENOMEM) {
      log_error(
        "PLAYER: There seems to be an issue with card [%d], ignoring.",
        current_info->card_index);

      return alsa_soundc_get_next_info(info);
    }
  }
  return error_r;
}

const char*
alsa_soundc_get_hardware_id(const struct sound_card_info *info) {
  assert(info != NULL);
  assert(info->hardware_id != NULL);
  return info->hardware_id;
}

const char*
alsa_soundc_get_long_name(const struct sound_card_info *info) {
  assert(info != NULL);
  assert(info->long_name != NULL);
  return info->long_name;
}

const char*
alsa_soundc_get_driver_name(const struct sound_card_info *info) {
  assert(info != NULL);
  return IF_NULL(info->driver_name, "unknown");
}

const char*
alsa_soundc_get_mixer_name(const struct sound_card_info *info) {
  assert(info != NULL);
  return IF_NULL(info->mixer_name, "N/A");
}

const char*
alsa_soundc_get_components(const struct sound_card_info *info) {
  assert(info != NULL);
  return IF_NULL(info->components, "");
}

void
alsa_soundc_release(struct sound_card_info **info) {
  assert(info != NULL);
  if (*info != NULL) {
    free_sound_card_info(*info);
    *info = NULL;
  }
}

bool
alsa_soundc_is_valid_hardware_id(const char *hardware_id) {
  snd_ctl_t *ctlp = NULL;
  error_t error_r = snd_ctl_open(&ctlp, hardware_id, SND_CTL_NONBLOCK);
  if (error_r != 0) {
    return false;
  } else {
    snd_ctl_close(ctlp);
    return true;
  }
}
