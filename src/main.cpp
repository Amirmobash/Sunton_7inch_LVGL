#include <Arduino.h>
#include <Preferences.h>
#include <lvgl.h>
#include "LGFX_Sunton_8048S070C.h"

/* ===================== PINS (EDIT) ===================== */
static constexpr gpio_num_t PIN_SENSOR_IN = GPIO_NUM_10;  // PC817 output -> ESP32 GPIO
static constexpr gpio_num_t PIN_MOTOR_OUT = GPIO_NUM_12;  // ESP32 GPIO -> motor driver

static constexpr bool SENSOR_ACTIVE_LOW = true;           // PC817 + pullup typically active-low
static constexpr bool MOTOR_ACTIVE_HIGH = true;           // typical driver active-high

/* Safety */
static constexpr uint32_t NO_PULSE_TIMEOUT_MS = 5000;
static constexpr uint32_t MIN_PULSE_GAP_US_HARD = 500;

/* ===================== DISPLAY/LVGL ===================== */
static const uint16_t SCREEN_W = 800;
static const uint16_t SCREEN_H = 480;

static LGFX gfx;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[SCREEN_W * 12];
static lv_color_t buf2[SCREEN_W * 12];

/* Fonts (ASCII only -> default OK) */
static const lv_font_t* F16 = LV_FONT_DEFAULT;
static const lv_font_t* F24 = LV_FONT_DEFAULT;
static const lv_font_t* F48 = LV_FONT_DEFAULT;   // اگر لازم شد در lv_conf.h فعالش کن

/* ===================== THEME ===================== */
static lv_color_t C_ORANGE;
static lv_color_t C_WHITE;
static lv_color_t C_BLACK;
static lv_color_t C_GRAY;
static lv_color_t C_RED;

/* ===================== PERSISTENCE ===================== */
static Preferences prefs;
static const char* NVS_NS    = "bandware";
static const char* KEY_ZIEL  = "ziel";
static const char* KEY_DEBMS = "debms";

/* ===================== STATE ===================== */
enum class State : uint8_t { IDLE, RUNNING, DONE, STOPPED, ERROR };

static volatile uint32_t isr_count = 0;
static volatile uint32_t isr_last_us = 0;

static uint32_t ist = 0;
static uint32_t ziel = 120;
static uint16_t deb_ms = 5;

static State st = State::IDLE;
static bool motor_on = false;
static uint32_t last_pulse_seen_ms = 0;
static char err_msg[128] = {0};

/* ===================== UI Screens ===================== */
static lv_obj_t* scr_main = nullptr;
static lv_obj_t* scr_set  = nullptr;
static lv_obj_t* scr_done = nullptr;
static lv_obj_t* scr_err  = nullptr;

/* Main widgets */
static lv_obj_t* lbl_ist_big = nullptr;
static lv_obj_t* lbl_ziel_big = nullptr;
static lv_obj_t* lbl_status = nullptr;
static lv_obj_t* bar = nullptr;

/* Settings widgets */
static lv_obj_t* ta_ziel = nullptr;
static lv_obj_t* ta_deb  = nullptr;
static lv_obj_t* kb      = nullptr;
static lv_obj_t* btn_clear = nullptr;

/* Done/Error */
static lv_obj_t* lbl_done = nullptr;
static lv_obj_t* lbl_err  = nullptr;

/* ===================== HW helpers ===================== */
static inline void motorWrite(bool on)
{
  motor_on = on;
  if (MOTOR_ACTIVE_HIGH) digitalWrite((int)PIN_MOTOR_OUT, on ? HIGH : LOW);
  else                   digitalWrite((int)PIN_MOTOR_OUT, on ? LOW : HIGH);
}

static void saveSettings()
{
  prefs.putUInt(KEY_ZIEL, ziel);
  prefs.putUShort(KEY_DEBMS, deb_ms);
}

static void loadSettings()
{
  ziel   = prefs.getUInt(KEY_ZIEL, 120);
  deb_ms = prefs.getUShort(KEY_DEBMS, 5);

  if (ziel < 1) ziel = 1;
  if (ziel > 999999) ziel = 999999;
  if (deb_ms < 1) deb_ms = 1;
  if (deb_ms > 100) deb_ms = 100;
}

static const char* stateText(State s)
{
  switch (s) {
    case State::IDLE:    return "Bereit";
    case State::RUNNING: return "Laeuft...";
    case State::DONE:    return "Fertig";
    case State::STOPPED: return "Stopp";
    case State::ERROR:   return "Fehler";
  }
  return "";
}

/* ===================== ISR ===================== */
static void IRAM_ATTR sensor_isr()
{
  const uint32_t now = (uint32_t)esp_timer_get_time(); // us

  if ((uint32_t)(now - isr_last_us) < MIN_PULSE_GAP_US_HARD) return;

  const uint32_t min_delta = (uint32_t)deb_ms * 1000UL;
  if ((uint32_t)(now - isr_last_us) < min_delta) return;

  isr_last_us = now;
  isr_count++;
}

/* ===================== LVGL glue ===================== */
static void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
  if (gfx.getStartCount() == 0) gfx.startWrite();
  gfx.pushImageDMA(area->x1, area->y1,
                   area->x2 - area->x1 + 1,
                   area->y2 - area->y1 + 1,
                   (lgfx::rgb565_t *)&color_p->full);
  lv_disp_flush_ready(disp);
}

static void my_touchpad_read(lv_indev_drv_t*, lv_indev_data_t *data)
{
  uint16_t x, y;
  data->state = LV_INDEV_STATE_REL;
  if (gfx.getTouch(&x, &y)) {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = x;
    data->point.y = y;
  }
}

/* ===================== UI helpers ===================== */
static void style_screen(lv_obj_t* scr)
{
  lv_obj_set_style_bg_color(scr, C_WHITE, 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(scr, C_BLACK, 0);
  lv_obj_set_style_pad_all(scr, 0, 0);
}

static lv_obj_t* make_header(lv_obj_t* scr, const char* title, const char* subtitle)
{
  lv_obj_t* head = lv_obj_create(scr);
  lv_obj_set_size(head, 800, 70);
  lv_obj_align(head, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_bg_color(head, C_ORANGE, 0);
  lv_obj_set_style_bg_opa(head, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(head, 0, 0);
  lv_obj_set_style_pad_left(head, 18, 0);
  lv_obj_set_style_pad_top(head, 10, 0);

  lv_obj_t* t = lv_label_create(head);
  lv_label_set_text(t, title);
  lv_obj_set_style_text_color(t, C_WHITE, 0);
  lv_obj_set_style_text_font(t, F24, 0);
  lv_obj_align(t, LV_ALIGN_LEFT_MID, 0, -12);

  lv_obj_t* s = lv_label_create(head);
  lv_label_set_text(s, subtitle);
  lv_obj_set_style_text_color(s, C_WHITE, 0);
  lv_obj_set_style_text_font(s, F16, 0);
  lv_obj_align(s, LV_ALIGN_LEFT_MID, 0, 16);

  return head;
}

static lv_obj_t* make_btn_fill(lv_obj_t* parent, const char* txt, lv_coord_t w, lv_coord_t h, lv_color_t bg, lv_color_t fg)
{
  lv_obj_t* btn = lv_btn_create(parent);
  lv_obj_set_size(btn, w, h);
  lv_obj_set_style_bg_color(btn, bg, 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(btn, 0, 0);
  lv_obj_set_style_radius(btn, 16, 0);

  lv_obj_t* l = lv_label_create(btn);
  lv_label_set_text(l, txt);
  lv_obj_set_style_text_color(l, fg, 0);
  lv_obj_set_style_text_font(l, F24, 0);
  lv_obj_center(l);
  return btn;
}

static lv_obj_t* make_btn_outline(lv_obj_t* parent, const char* txt, lv_coord_t w, lv_coord_t h)
{
  lv_obj_t* btn = lv_btn_create(parent);
  lv_obj_set_size(btn, w, h);
  lv_obj_set_style_bg_color(btn, C_WHITE, 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(btn, 3, 0);
  lv_obj_set_style_border_color(btn, C_ORANGE, 0);
  lv_obj_set_style_radius(btn, 16, 0);

  lv_obj_t* l = lv_label_create(btn);
  lv_label_set_text(l, txt);
  lv_obj_set_style_text_color(l, C_ORANGE, 0);
  lv_obj_set_style_text_font(l, F24, 0);
  lv_obj_center(l);
  return btn;
}

static void go(lv_obj_t* scr, lv_scr_load_anim_t anim)
{
  lv_scr_load_anim(scr, anim, 220, 0, false);
}

/* ===================== UI updates ===================== */
static void sync_count()
{
  uint32_t p;
  noInterrupts(); p = isr_count; interrupts();
  ist = p;
}

static void update_main_ui()
{
  lv_label_set_text_fmt(lbl_ist_big, "%lu", (unsigned long)ist);
  lv_label_set_text_fmt(lbl_ziel_big, "%lu", (unsigned long)ziel);
  lv_label_set_text_fmt(lbl_status, "Status: %s", stateText(st));

  int pct = 0;
  if (ziel > 0) {
    uint32_t c = (ist > ziel) ? ziel : ist;
    pct = (int)((c * 100UL) / ziel);
  }
  lv_bar_set_value(bar, pct, LV_ANIM_ON);
}

/* ===================== Workflow ===================== */
static void set_error(const char* msg)
{
  motorWrite(false);
  st = State::ERROR;
  strncpy(err_msg, msg, sizeof(err_msg)-1);
  err_msg[sizeof(err_msg)-1] = 0;
  lv_label_set_text(lbl_err, err_msg);
  go(scr_err, LV_SCR_LOAD_ANIM_MOVE_LEFT);
}

static void process_workflow()
{
  sync_count();

  static uint32_t last_ist = 0;
  uint32_t now = millis();
  if (ist != last_ist) {
    last_ist = ist;
    last_pulse_seen_ms = now;
  }
  if (last_pulse_seen_ms == 0) last_pulse_seen_ms = now;

  if (st == State::RUNNING && ist >= ziel) {
    motorWrite(false);
    st = State::DONE;
    go(scr_done, LV_SCR_LOAD_ANIM_MOVE_LEFT);
    return;
  }

  if (st == State::RUNNING) {
    if (now - last_pulse_seen_ms > NO_PULSE_TIMEOUT_MS) {
      set_error("Fehler: Keine Impulse. Sensor/Band pruefen.");
      return;
    }
  }

  update_main_ui();
}

/* ===================== Callbacks ===================== */
static void on_start(lv_event_t*)
{
  if (st == State::ERROR) return;

  if (st == State::DONE) {
    noInterrupts(); isr_count = 0; interrupts();
    ist = 0;
  }

  st = State::RUNNING;
  motorWrite(true);
  last_pulse_seen_ms = millis();
  update_main_ui();
}

static void on_stop(lv_event_t*)
{
  motorWrite(false);
  if (st == State::RUNNING) st = State::STOPPED;
  update_main_ui();
}

static void on_reset(lv_event_t*)
{
  motorWrite(false);
  noInterrupts(); isr_count = 0; interrupts();
  ist = 0;
  st = State::IDLE;
  update_main_ui();
}

static void on_open_settings(lv_event_t*)
{
  if (st == State::RUNNING) return;

  char tmp[16];
  snprintf(tmp, sizeof(tmp), "%lu", (unsigned long)ziel);
  lv_textarea_set_text(ta_ziel, tmp);
  snprintf(tmp, sizeof(tmp), "%u", (unsigned)deb_ms);
  lv_textarea_set_text(ta_deb, tmp);

  go(scr_set, LV_SCR_LOAD_ANIM_MOVE_LEFT);
}

static void on_clear_in_settings(lv_event_t*)
{
  // فقط در Settings: پاک کردن سريع Zielmenge
  if (!ta_ziel) return;
  lv_textarea_set_text(ta_ziel, "");
  lv_textarea_set_cursor_pos(ta_ziel, LV_TEXTAREA_CURSOR_LAST);
  if (kb) lv_keyboard_set_textarea(kb, ta_ziel);
  lv_obj_add_state(ta_ziel, LV_STATE_FOCUSED);
}

static void kb_event(lv_event_t* e)
{
  lv_event_code_t code = lv_event_get_code(e);

  if (code == LV_EVENT_READY) {
    uint32_t new_z = (uint32_t)strtoul(lv_textarea_get_text(ta_ziel), nullptr, 10);
    uint32_t new_d = (uint32_t)strtoul(lv_textarea_get_text(ta_deb), nullptr, 10);

    if (new_z < 1) new_z = 1;
    if (new_z > 999999) new_z = 999999;
    if (new_d < 1) new_d = 1;
    if (new_d > 100) new_d = 100;

    ziel = new_z;
    deb_ms = (uint16_t)new_d;
    saveSettings();

    go(scr_main, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
    update_main_ui();
  }

  if (code == LV_EVENT_CANCEL) {
    go(scr_main, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
    update_main_ui();
  }
}

/* ===================== Screens ===================== */
static void build_main()
{
  scr_main = lv_obj_create(nullptr);
  style_screen(scr_main);

  make_header(scr_main, "Bandware Zaehler", "Uebersicht: IST / Ziel + Start/Stop/Reset");

  lv_obj_t* frame = lv_obj_create(scr_main);
  lv_obj_set_size(frame, 780, 300);
  lv_obj_align(frame, LV_ALIGN_TOP_MID, 0, 78);
  lv_obj_set_style_radius(frame, 18, 0);
  lv_obj_set_style_border_width(frame, 2, 0);
  lv_obj_set_style_border_color(frame, C_ORANGE, 0);
  lv_obj_set_style_pad_all(frame, 16, 0);

  // IST left
  lv_obj_t* col_ist = lv_obj_create(frame);
  lv_obj_set_size(col_ist, 360, 185);
  lv_obj_align(col_ist, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_bg_opa(col_ist, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(col_ist, 0, 0);
  lv_obj_set_style_pad_all(col_ist, 0, 0);

  lv_obj_t* cap_ist = lv_label_create(col_ist);
  lv_label_set_text(cap_ist, "IST (Zaehlerstand)");
  lv_obj_set_style_text_font(cap_ist, F24, 0);
  lv_obj_align(cap_ist, LV_ALIGN_TOP_LEFT, 0, 0);

  lbl_ist_big = lv_label_create(col_ist);
  lv_label_set_text(lbl_ist_big, "0");
  lv_obj_set_style_text_font(lbl_ist_big, F48, 0);
  lv_obj_set_style_text_color(lbl_ist_big, C_ORANGE, 0);
  lv_obj_align(lbl_ist_big, LV_ALIGN_TOP_LEFT, 0, 55);

  // ZIEL right (big)
  lv_obj_t* col_z = lv_obj_create(frame);
  lv_obj_set_size(col_z, 380, 185);
  lv_obj_align(col_z, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_obj_set_style_bg_opa(col_z, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(col_z, 0, 0);
  lv_obj_set_style_pad_all(col_z, 0, 0);

  lv_obj_t* cap_z = lv_label_create(col_z);
  lv_label_set_text(cap_z, "Zielmenge (Ziel)");
  lv_obj_set_style_text_font(cap_z, F24, 0);
  lv_obj_align(cap_z, LV_ALIGN_TOP_LEFT, 0, 0);

  lbl_ziel_big = lv_label_create(col_z);
  lv_label_set_text(lbl_ziel_big, "120");
  lv_obj_set_style_text_font(lbl_ziel_big, F48, 0);
  lv_obj_set_style_text_color(lbl_ziel_big, C_BLACK, 0);
  lv_obj_align(lbl_ziel_big, LV_ALIGN_TOP_LEFT, 0, 55);

  // progress + status
  bar = lv_bar_create(frame);
  lv_obj_set_size(bar, 748, 28);
  lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, -52);
  lv_bar_set_range(bar, 0, 100);
  lv_obj_set_style_bg_color(bar, C_GRAY, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(bar, LV_OPA_20, LV_PART_MAIN);
  lv_obj_set_style_bg_color(bar, C_ORANGE, LV_PART_INDICATOR);

  lbl_status = lv_label_create(frame);
  lv_label_set_text(lbl_status, "Status: Bereit");
  lv_obj_set_style_text_font(lbl_status, F24, 0);
  lv_obj_align(lbl_status, LV_ALIGN_BOTTOM_LEFT, 0, -10);

  // Bottom buttons row
  lv_obj_t* bottom = lv_obj_create(scr_main);
  lv_obj_set_size(bottom, 800, 92);
  lv_obj_align(bottom, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_opa(bottom, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(bottom, 0, 0);
  lv_obj_set_style_pad_all(bottom, 10, 0);

  const int bw = 185;
  const int bh = 72;
  const int gap = 10;

  lv_obj_t* bstart = make_btn_fill(bottom, "START", bw, bh, C_ORANGE, C_WHITE);
  lv_obj_align(bstart, LV_ALIGN_LEFT_MID, 0, 0);
  lv_obj_add_event_cb(bstart, [](lv_event_t*){ on_start(nullptr); }, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* bstop = make_btn_outline(bottom, "STOP", bw, bh);
  lv_obj_align(bstop, LV_ALIGN_LEFT_MID, bw + gap, 0);
  lv_obj_add_event_cb(bstop, [](lv_event_t*){ on_stop(nullptr); }, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* breset = make_btn_outline(bottom, "RESET", bw, bh);
  lv_obj_align(breset, LV_ALIGN_LEFT_MID, (bw + gap) * 2, 0);
  lv_obj_add_event_cb(breset, [](lv_event_t*){ on_reset(nullptr); }, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* bset = make_btn_fill(bottom, "EINSTELL.", bw, bh, C_ORANGE, C_WHITE);
  lv_obj_align(bset, LV_ALIGN_LEFT_MID, (bw + gap) * 3, 0);
  lv_obj_add_event_cb(bset, on_open_settings, LV_EVENT_CLICKED, nullptr);
}

static void build_settings()
{
  scr_set = lv_obj_create(nullptr);
  style_screen(scr_set);

  make_header(scr_set, "Einstellungen", "Ziel und Entprellung setzen und speichern");

  // Card area (left side), keyboard bottom
  lv_obj_t* card = lv_obj_create(scr_set);
  lv_obj_set_size(card, 780, 250);
  lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 78);
  lv_obj_set_style_radius(card, 18, 0);
  lv_obj_set_style_border_width(card, 2, 0);
  lv_obj_set_style_border_color(card, C_ORANGE, 0);
  lv_obj_set_style_pad_all(card, 18, 0);

  // Ziel row + CLEAR button (HERE ONLY)
  lv_obj_t* l1 = lv_label_create(card);
  lv_label_set_text(l1, "Zielmenge:");
  lv_obj_set_style_text_font(l1, F24, 0);
  lv_obj_align(l1, LV_ALIGN_TOP_LEFT, 0, 0);

  ta_ziel = lv_textarea_create(card);
  lv_obj_set_size(ta_ziel, 420, 60);
  lv_obj_align(ta_ziel, LV_ALIGN_TOP_LEFT, 0, 45);
  lv_textarea_set_one_line(ta_ziel, true);
  lv_obj_set_style_text_font(ta_ziel, F24, 0);

  btn_clear = make_btn_fill(card, "CLEAR", 150, 60, C_RED, C_WHITE);
  lv_obj_align(btn_clear, LV_ALIGN_TOP_RIGHT, 0, 45);
  lv_obj_add_event_cb(btn_clear, [](lv_event_t*){ on_clear_in_settings(nullptr); }, LV_EVENT_CLICKED, nullptr);

  // Debounce row
  lv_obj_t* l2 = lv_label_create(card);
  lv_label_set_text(l2, "Entprellung (ms):");
  lv_obj_set_style_text_font(l2, F24, 0);
  lv_obj_align(l2, LV_ALIGN_TOP_LEFT, 0, 120);

  ta_deb = lv_textarea_create(card);
  lv_obj_set_size(ta_deb, 420, 60);
  lv_obj_align(ta_deb, LV_ALIGN_TOP_LEFT, 0, 165);
  lv_textarea_set_one_line(ta_deb, true);
  lv_obj_set_style_text_font(ta_deb, F24, 0);

  kb = lv_keyboard_create(scr_set);
  lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);
  lv_obj_set_size(kb, 780, 180);
  lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_add_event_cb(kb, kb_event, LV_EVENT_ALL, nullptr);
  lv_keyboard_set_textarea(kb, ta_ziel);

  lv_obj_add_event_cb(ta_ziel, [](lv_event_t*){ lv_keyboard_set_textarea(kb, ta_ziel); }, LV_EVENT_FOCUSED, nullptr);
  lv_obj_add_event_cb(ta_deb,  [](lv_event_t*){ lv_keyboard_set_textarea(kb, ta_deb ); }, LV_EVENT_FOCUSED, nullptr);
}

static void build_done()
{
  scr_done = lv_obj_create(nullptr);
  style_screen(scr_done);

  make_header(scr_done, "Fertig", "Ziel erreicht: Motor AUS, Band entnehmen");

  lv_obj_t* card = lv_obj_create(scr_done);
  lv_obj_set_size(card, 780, 260);
  lv_obj_align(card, LV_ALIGN_CENTER, 0, 10);
  lv_obj_set_style_radius(card, 18, 0);
  lv_obj_set_style_bg_color(card, C_ORANGE, 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(card, 0, 0);

  lbl_done = lv_label_create(card);
  lv_label_set_text(lbl_done, "Fertig!\nBitte Band entnehmen.");
  lv_obj_set_style_text_color(lbl_done, C_WHITE, 0);
  lv_obj_set_style_text_font(lbl_done, F24, 0);
  lv_obj_center(lbl_done);

  lv_obj_t* btn_ok = make_btn_outline(scr_done, "OK", 300, 70);
  lv_obj_align(btn_ok, LV_ALIGN_BOTTOM_MID, 0, -20);
  lv_obj_add_event_cb(btn_ok, [](lv_event_t*){
    st = State::IDLE;
    go(scr_main, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
    update_main_ui();
  }, LV_EVENT_CLICKED, nullptr);
}

static void build_error()
{
  scr_err = lv_obj_create(nullptr);
  style_screen(scr_err);

  make_header(scr_err, "Fehler", "Failsafe: Motor AUS. Ursache pruefen und Reset");

  lv_obj_t* card = lv_obj_create(scr_err);
  lv_obj_set_size(card, 780, 260);
  lv_obj_align(card, LV_ALIGN_CENTER, 0, 10);
  lv_obj_set_style_radius(card, 18, 0);
  lv_obj_set_style_bg_color(card, C_RED, 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(card, 0, 0);

  lbl_err = lv_label_create(card);
  lv_label_set_text(lbl_err, "Fehler!");
  lv_obj_set_style_text_color(lbl_err, C_WHITE, 0);
  lv_obj_set_style_text_font(lbl_err, F24, 0);
  lv_obj_center(lbl_err);

  lv_obj_t* btn_r = make_btn_outline(scr_err, "RESET", 300, 70);
  lv_obj_align(btn_r, LV_ALIGN_BOTTOM_MID, 0, -20);
  lv_obj_add_event_cb(btn_r, [](lv_event_t*){
    motorWrite(false);
    err_msg[0] = 0;
    st = State::IDLE;
    noInterrupts(); isr_count = 0; interrupts();
    ist = 0;
    go(scr_main, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
    update_main_ui();
  }, LV_EVENT_CLICKED, nullptr);
}

/* ===================== Setup/Loop ===================== */
void setup()
{
  Serial.begin(115200);
  delay(200);

  pinMode((int)PIN_MOTOR_OUT, OUTPUT);
  motorWrite(false);

  if (SENSOR_ACTIVE_LOW) pinMode((int)PIN_SENSOR_IN, INPUT_PULLUP);
  else                   pinMode((int)PIN_SENSOR_IN, INPUT_PULLDOWN);

  prefs.begin(NVS_NS, false);
  loadSettings();

  gfx.begin();
  gfx.setBrightness(180);

  lv_init();

  lv_disp_draw_buf_init(&draw_buf, buf1, buf2, SCREEN_W * 12);

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

  C_ORANGE = lv_palette_main(LV_PALETTE_ORANGE);
  C_WHITE  = lv_color_white();
  C_BLACK  = lv_color_black();
  C_GRAY   = lv_color_make(40, 40, 40);
  C_RED    = lv_palette_main(LV_PALETTE_RED);

  build_main();
  build_settings();
  build_done();
  build_error();

  lv_scr_load(scr_main);

  st = State::IDLE;
  noInterrupts(); isr_count = 0; interrupts();
  ist = 0;
  update_main_ui();

  // init settings page textareas (values)
  // (after build_settings)
  char tmp[16];
  snprintf(tmp, sizeof(tmp), "%lu", (unsigned long)ziel);
  lv_textarea_set_text(ta_ziel, tmp);
  snprintf(tmp, sizeof(tmp), "%u", (unsigned)deb_ms);
  lv_textarea_set_text(ta_deb, tmp);

  attachInterrupt((int)PIN_SENSOR_IN, sensor_isr, SENSOR_ACTIVE_LOW ? FALLING : RISING);
}

void loop()
{
  lv_timer_handler();
  delay(5);

  if (st == State::ERROR && motor_on) motorWrite(false);

  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last >= 80) {
    last = now;
    if (st != State::ERROR) process_workflow();
  }
}
