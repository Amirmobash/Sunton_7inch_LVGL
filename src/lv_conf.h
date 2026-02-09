#pragma once

/* ===================== Color & Pixel Format ===================== */
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0   /* Keep 0 for RGB565 pushImageDMA in LovyanGFX */

/* ===================== Tick Source (Arduino millis) ===================== */
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

/* ===================== Memory ===================== */
/* 140 KB is usually OK for ESP32-S3 + PSRAM + 800x480 with partial buffers */
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (140U * 1024U)

/* ===================== Logging ===================== */
#define LV_USE_LOG 0

/* ===================== Default Fonts (LVGL built-in) ===================== */
#define LV_FONT_MONTSERRAT_14  1
#define LV_FONT_MONTSERRAT_24  1
#define LV_FONT_MONTSERRAT_48  1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* ===================== Text / Language ===================== */
/* Not needed for German if you use ASCII (ae/oe/ue) */
#define LV_USE_BIDI 0

/* ===================== Performance / Rendering ===================== */
#define LV_USE_GPU_ESP32 0
#define LV_USE_DRAW_SW 1

/* Enable smooth animations (progress bar, screen transitions) */
#define LV_USE_ANIMATION 1

/* ===================== Input Devices ===================== */
#define LV_USE_INDEV 1

/* ===================== Widgets (enable what you use) ===================== */
#define LV_USE_BAR       1
#define LV_USE_BTN       1
#define LV_USE_LABEL     1
#define LV_USE_TEXTAREA  1
#define LV_USE_KEYBOARD  1

/* ===================== Extra: Optional but helpful ===================== */
/* If you ever see strange text encoding issues, keep this enabled (default in v8) */
#define LV_TXT_ENC LV_TXT_ENC_UTF8

/* ===================== Debug Safety ===================== */
/* Turn on if you want to catch memory issues (costs CPU) */
/* #define LV_USE_ASSERT_NULL 1 */
/* #define LV_USE_ASSERT_MEM  1 */
/* #define LV_USE_ASSERT_OBJ  1 */
