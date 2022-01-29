#pragma once
#include "shrdef.h"

struct sound_card_info;


//
// Setup
//

error_t
alsa_soundc_get_next_info(struct sound_card_info **info);

void
alsa_soundc_release(struct sound_card_info **info);


//
// Query
//

bool
alsa_soundc_is_valid_hardware_id(const char *hardware_id);

const char*
alsa_soundc_get_hardware_id(const struct sound_card_info *info);

const char*
alsa_soundc_get_long_name(const struct sound_card_info *info);

const char*
alsa_soundc_get_driver_name(const struct sound_card_info *info);

const char*
alsa_soundc_get_mixer_name(const struct sound_card_info *info);

const char*
alsa_soundc_get_components(const struct sound_card_info *info);
