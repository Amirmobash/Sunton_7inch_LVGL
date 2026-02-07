#include <Arduino.h>
#include <lvgl.h>
#include "LGFX_Sunton_8048S070C.h"

static const uint16_t SCREEN_W = 800;
static const uint16_t SCREEN_H = 480;

static LGFX gfx;

// بافر رسم: 10 خط * عرض صفحه (دو بافر برای DMA/پرفورمنس)
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[SCREEN_W * 10];
static lv_color_t buf2[SCREEN_W * 10];

static void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
  if (gfx.getStartCount() == 0) gfx.startWrite();

  gfx.pushImageDMA(
      area->x1, area->y1,
      area->x2 - area->x1 + 1,
      area->y2 - area->y1 + 1,
      (lgfx::rgb565_t *)&color_p->full);

  lv_disp_flush_ready(disp);
}

static void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
  uint16_t x, y;
  data->state = LV_INDEV_STATE_REL;

  if (gfx.getTouch(&x, &y))
  {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = x;
    data->point.y = y;
  }
}

static lv_obj_t* label;

static void btn_event_cb(lv_event_t *e)
{
  if (lv_event_get_code(e) == LV_EVENT_CLICKED)
  {
    static uint32_t n = 0;
    n++;
    lv_label_set_text_fmt(label, "Clicked: %lu", (unsigned long)n);
  }
}

void setup()
{
  Serial.begin(115200);
  delay(200);

  gfx.begin();
  gfx.setBrightness(160); // 0..255

  lv_init();

  lv_disp_draw_buf_init(&draw_buf, buf1, buf2, SCREEN_W * 10);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = SCREEN_W;
  disp_drv.ver_res = SCREEN_H;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);

  // UI ساده
  label = lv_label_create(lv_scr_act());
  lv_label_set_text(label, "Hello Sunton/Jingcai 7inch!");
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);

  lv_obj_t *btn = lv_btn_create(lv_scr_act());
  lv_obj_set_size(btn, 240, 90);
  lv_obj_align(btn, LV_ALIGN_CENTER, 0, 0);
  lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_ALL, nullptr);

  lv_obj_t *btn_lbl = lv_label_create(btn);
  lv_label_set_text(btn_lbl, "Touch / Click");
  lv_obj_center(btn_lbl);
}

void loop()
{
  lv_timer_handler();
  delay(5);
}
