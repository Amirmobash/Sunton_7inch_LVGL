#pragma once

/* رنگ 16 بیت برای RGB565 */
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

/* Tick با millis() در آردوینو */
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

/* لاگ خاموش */
#define LV_USE_LOG 0

/* حافظه داخلی LVGL */
#define LV_MEM_SIZE (64U * 1024U)
