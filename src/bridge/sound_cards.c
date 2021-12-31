#include "alsa_soundc.h"
#include "main.h"

error_t
list_sound_cards() {
  struct sound_card_info* card_info = NULL;
  error_t error_r = alsa_soundc_get_next_info(&card_info);
  if (card_info != NULL) {
    log_info("Available sound cards:");
    while (error_r == 0 && card_info != NULL) {
      log_info("");
      log_info(
        "Hardware [%s] with driver [%s]",
        alsa_soundc_get_hardware_id(card_info),
        alsa_soundc_get_driver_name(card_info));
      log_info(
        "Name: %s",
        alsa_soundc_get_long_name(card_info));
      log_info(
        "Mixer: %s",
        alsa_soundc_get_mixer_name(card_info));
      log_info(
        "Control components: %s",
        alsa_soundc_get_components(card_info));

      error_r = alsa_soundc_get_next_info(&card_info);
    }
  }  else {
    log_info("No valid sound hardware found, please check /proc/asound/cards");
  }
  return error_r;
}
