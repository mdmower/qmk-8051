// Copyright 2024 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "action_layer.h" // layer_state_t

#ifndef EECONFIG_MAGIC_NUMBER
#    define EECONFIG_MAGIC_NUMBER (uint16_t)0xFEE3 // When changing, decrement this value to avoid future re-init issues
#endif
#define EECONFIG_MAGIC_NUMBER_OFF (uint16_t)0xFFFF

void nvm_eeconfig_erase(void);

bool nvm_eeconfig_is_enabled(void);
bool nvm_eeconfig_is_disabled(void);

void nvm_eeconfig_enable(void);
void nvm_eeconfig_disable(void);

union debug_config_t;
void                         nvm_eeconfig_read_debug(union debug_config_t *debug_config);
void                         nvm_eeconfig_update_debug(const union debug_config_t *debug_config);

layer_state_t nvm_eeconfig_read_default_layer(void);
void          nvm_eeconfig_update_default_layer(layer_state_t state);

union keymap_config_t;
void                          nvm_eeconfig_read_keymap(union keymap_config_t *keymap_config);
void                          nvm_eeconfig_update_keymap(const union keymap_config_t *keymap_config);

#ifdef AUDIO_ENABLE
union audio_config_t;
void                         nvm_eeconfig_read_audio(union audio_config_t *audio_config);
void                         nvm_eeconfig_update_audio(const union audio_config_t *audio_config);
#endif // AUDIO_ENABLE

#ifdef UNICODE_COMMON_ENABLE
union unicode_config_t;
void                           nvm_eeconfig_read_unicode_mode(union unicode_config_t *unicode_config);
void                           nvm_eeconfig_update_unicode_mode(const union unicode_config_t *unicode_config);
#endif // UNICODE_COMMON_ENABLE

#ifdef BACKLIGHT_ENABLE
union backlight_config_t;
void                             nvm_eeconfig_read_backlight(union backlight_config_t *backlight_config);
void                             nvm_eeconfig_update_backlight(const union backlight_config_t *backlight_config);
#endif // BACKLIGHT_ENABLE

#ifdef STENO_ENABLE
uint8_t nvm_eeconfig_read_steno_mode(void);
void    nvm_eeconfig_update_steno_mode(uint8_t val);
#endif // STENO_ENABLE

#ifdef RGB_MATRIX_ENABLE
union rgb_config_t;
void                       nvm_eeconfig_read_rgb_matrix(union rgb_config_t *rgb_matrix_config);
void                       nvm_eeconfig_update_rgb_matrix(const union rgb_config_t *rgb_matrix_config);
#endif

#ifdef LED_MATRIX_ENABLE
union led_eeconfig_t;
void                         nvm_eeconfig_read_led_matrix(union led_eeconfig_t *led_matrix_config);
void                         nvm_eeconfig_update_led_matrix(const union led_eeconfig_t *led_matrix_config);
#endif // LED_MATRIX_ENABLE

#ifdef RGBLIGHT_ENABLE
union rgblight_config_t;
void                            nvm_eeconfig_read_rgblight(union rgblight_config_t *rgblight_config);
void                            nvm_eeconfig_update_rgblight(const union rgblight_config_t *rgblight_config);
#endif // RGBLIGHT_ENABLE

#if (EECONFIG_KB_DATA_SIZE) == 0
uint32_t nvm_eeconfig_read_kb(void);
void     nvm_eeconfig_update_kb(uint32_t val);
#endif // (EECONFIG_KB_DATA_SIZE) == 0

#if (EECONFIG_USER_DATA_SIZE) == 0
uint32_t nvm_eeconfig_read_user(void);
void     nvm_eeconfig_update_user(uint32_t val);
#endif // (EECONFIG_USER_DATA_SIZE) == 0

#ifdef HAPTIC_ENABLE
union haptic_config_t;
void                          nvm_eeconfig_read_haptic(union haptic_config_t *haptic_config);
void                          nvm_eeconfig_update_haptic(const union haptic_config_t *haptic_config);
#endif // HAPTIC_ENABLE

#ifdef CONNECTION_ENABLE
union connection_config_t;
void                              nvm_eeconfig_read_connection(union connection_config_t *config);
void                              nvm_eeconfig_update_connection(const union connection_config_t *config);
#endif // CONNECTION_ENABLE

bool nvm_eeconfig_read_handedness(void);
void nvm_eeconfig_update_handedness(bool val);

#if (EECONFIG_KB_DATA_SIZE) > 0
bool     nvm_eeconfig_is_kb_datablock_valid(void);
uint32_t nvm_eeconfig_read_kb_datablock(void *data, uint32_t offset, uint32_t length);
uint32_t nvm_eeconfig_update_kb_datablock(const void *data, uint32_t offset, uint32_t length);
void     nvm_eeconfig_init_kb_datablock(void);
#endif // (EECONFIG_KB_DATA_SIZE) > 0

#if (EECONFIG_USER_DATA_SIZE) > 0
bool     nvm_eeconfig_is_user_datablock_valid(void);
uint32_t nvm_eeconfig_read_user_datablock(void *data, uint32_t offset, uint32_t length);
uint32_t nvm_eeconfig_update_user_datablock(const void *data, uint32_t offset, uint32_t length);
void     nvm_eeconfig_init_user_datablock(void);
#endif // (EECONFIG_USER_DATA_SIZE) > 0
