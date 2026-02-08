#pragma once

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

#define LV_MEM_SIZE (140U * 1024U)
#define LV_USE_LOG 0

/* فونت‌های پیش‌فرض LVGL */
#define LV_FONT_MONTSERRAT_14  1
#define LV_FONT_MONTSERRAT_24  1
#define LV_FONT_MONTSERRAT_48  1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* (اختیاری) اگر خواستی، می‌تونی bidi رو خاموش نگه داری؛ برای آلمانی لازم نیست */
#define LV_USE_BIDI 0
