# Bandware Counter für Sunton ESP32-S3 (7" Display)

## 📋 Technische Daten
- **Mikrocontroller**: ESP32-S3 (240 MHz, 16 MB Flash, 8 MB PSRAM)
- **Display**: 7 Zoll TN‑Panel, 800×480 Pixel, RGB565‑Schnittstelle
- **Touch**: kapazitiv, GT911 Controller (I2C)
- **Anschlüsse**: USB‑C, SD‑Kartensteckplatz, I2S Audio‑Ausgang
- **Besonderheit**: parallele RGB‑Schnittstelle (kein SPI)

---

## 🚀 Projektübersicht
Das Projekt realisiert einen **Bandzähler** für industrielle Anwendungen:
- Ein externer Sensor (z. B. PC817) liefert Impulse pro Band‑Stück.
- Ein Relais oder MOSFET steuert den Bandantrieb (Motor).
- Ein grafisches Touch‑Interface (LVGL) zeigt Ist‑Stand, Zielmenge, Status und Fehlermeldungen an.
- Alle Einstellungen werden persistent in der NVS gespeichert.

**Hardware‑Pins:**
| Funktion   | GPIO |
|------------|------|
| Sensor‑Eingang | 17 (P5) |
| Motor‑Ausgang  | 12 (P2) |
| Touch SDA      | 19    |
| Touch SCL      | 20    |
| Touch Reset    | 38    |
| Backlight      | 2     |

---

## ⚙️ PlatformIO Projekt einrichten

### `platformio.ini`
```ini
[env:sunton_s3]
platform = espressif32@6.9.0
board = sunton_s3
framework = arduino

upload_port = COM4
monitor_port = COM4
monitor_speed = 115200
monitor_filters = esp32_exception_decoder

build_unflags =
  -std=gnu++11

build_flags =
  -Os
  -std=gnu++17
  -DCORE_DEBUG_LEVEL=3
  -DLGFX_USE_V1
  -DLV_CONF_INCLUDE_SIMPLE
  -DLV_CONF_SUPPRESS_DEFINE_CHECK
  -I./src

lib_deps =
  https://github.com/lovyan03/LovyanGFX.git#1.1.7
  lvgl/lvgl@8.3.7
```

**Wichtig:**  
- Das Board `sunton_s3` ist in der PlatformIO‑Board‑Liste enthalten und konfiguriert PSRAM sowie Flash‑Mode richtig.  
- Die Compiler‑Flags `-DLV_CONF_INCLUDE_SIMPLE` und `-I./src` erlauben es, die `lv_conf.h` im Projekt‑Ordner `src` zu platzieren.

---

## 📁 Projektstruktur (Auszug)

```
projekt/
├── platformio.ini
├── README.md
├── src/
│   ├── main.cpp
│   ├── LGFX_Sunton_8048S070C.h    # LovyanGFX‑Treiber
│   └── lv_conf.h                   # LVGL‑Konfiguration
└── ...
```

---

## 🔌 Hardware‑Treiber: `LGFX_Sunton_8048S070C.h`

Die Datei enthält die vollständige LovyanGFX‑Konfiguration für das Sunton‑Display:

```cpp
#pragma once
#include <Arduino.h>
#include <driver/i2c.h>
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>

class LGFX : public lgfx::LGFX_Device
{
public:
  lgfx::Bus_RGB     _bus_instance;
  lgfx::Panel_RGB   _panel_instance;
  lgfx::Light_PWM   _light_instance;
  lgfx::Touch_GT911 _touch_instance;

  LGFX(void) { ... }   // hier werden alle Pins und Timings gesetzt
};
```

- RGB‑Datenpins: `GPIO 15,7,6,5,4,9,46,3,8,16,1,14,21,47,48,45`  
- Steuerpins: `HSYNC=39, VSYNC=40, DE=41, PCLK=42`  
- Touch: I2C‑Port 0, SDA=19, SCL=20, Reset=38  
- Backlight: GPIO 2

---

## 🧠 LVGL‑Konfiguration (`lv_conf.h`)

Die wichtigsten Einstellungen:

```c
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

#define LV_MEM_SIZE (140U * 1024U)

#define LV_FONT_MONTSERRAT_14  1
#define LV_FONT_MONTSERRAT_24  1
#define LV_FONT_MONTSERRAT_48  1
#define LV_FONT_DEFAULT &lv_font_montserrat_14
```

---

## 📄 Hauptprogramm (`main.cpp`) – Kernfunktionen

Der Code realisiert einen Zustandsautomaten mit den Zuständen:
- `IDLE` – bereit
- `RUNNING` – Motor läuft, Zähler zählt Impulse
- `DONE` – Ziel erreicht
- `STOPPED` – manuell gestoppt
- `ERROR` – Fehler (keine Impulse, Sensor defekt)

**Wichtige Teile:**

```cpp
#include <Arduino.h>
#include <Preferences.h>
#include <lvgl.h>
#include "LGFX_Sunton_8048S070C.h"

static constexpr gpio_num_t PIN_SENSOR_IN = GPIO_NUM_17;   // P5: IO17
static constexpr gpio_num_t PIN_MOTOR_OUT = GPIO_NUM_12;   // P2: IO12

static constexpr bool SENSOR_ACTIVE_LOW = false;   // PC817 open‑collector + Pullup → active LOW
static constexpr bool MOTOR_ACTIVE_HIGH = true;    // Relais / MOSFET IN active HIGH

static constexpr uint32_t NO_PULSE_TIMEOUT_MS = 5000;   // Lauf ohne Impuls → Fehler
static constexpr uint32_t MIN_PULSE_GAP_US_HARD = 500;  // Entprellung

static volatile uint32_t isr_count = 0;

static void IRAM_ATTR sensor_isr() {
  const uint32_t now = esp_timer_get_time();
  if ((now - isr_last_us) < MIN_PULSE_GAP_US_HARD) return;
  // zusätzliche software‑Entprellung über deb_ms (aus Einstellungen)
  if ((now - isr_last_us) < (uint32_t)deb_ms * 1000UL) return;
  isr_last_us = now;
  isr_count++;
}
```

- Die Einstellungen (Zielmenge, Entprellzeit) werden mit der `Preferences`‑Bibliothek gespeichert.  
- Das UI besteht aus vier Screens: Hauptbildschirm, Einstellungen, Fertig‑Screen, Fehler‑Screen.  
- Touch‑Ereignisse werden von LVGL über die `my_touchpad_read`‑Funktion ausgelesen.

---

## 🖱️ Bedienung des Zählers

| Button    | Funktion |
|-----------|----------|
| **START** | Motor ein, Zähler läuft (sofern nicht Fehler) |
| **STOP**  | Motor aus, Zustand auf `STOPPED` |
| **RESET** | Zählerstand = 0, Zustand auf `IDLE`, Motor aus |
| **EINSTELL.** | Öffnet den Einstellungsbildschirm (nur im IDLE‑Zustand) |

Im Einstellungsbildschirm:
- Zielmenge (1 … 999.999)
- Entprellzeit (1 … 100 ms)
- Ein virtuelles Zahlen‑Keyboard erleichtert die Eingabe.
- Mit **CLEAR** wird das Ziel‑Feld geleert.
- **ENTER** speichert und kehrt zum Hauptbildschirm zurück.

---

## 🔧 Erste Schritte & Upload

1. **PlattformIO** mit dem aktuellen Projektordner öffnen.
2. USB‑Kabel an den ESP32‑S3 anschließen.
3. **Board‑Auswahl:** `sunton_s3` (automatisch über `platformio.ini`).
4. **Upload:**  
   - Falls der Upload nicht startet, **BOOT**‑Taste gedrückt halten, **RST** kurz drücken, dann BOOT loslassen.
5. **Serieller Monitor:** 115200 Baud. Dort werden Startmeldungen und evtl. Fehler ausgegeben.

---

## ⚠️ Häufige Probleme und Lösungen

| Problem | Mögliche Ursache | Lösung |
|---------|------------------|--------|
| Display bleibt schwarz, Backlight leuchtet | Falsche RGB‑Pins oder Timing | Prüfe `LGFX_Sunton_8048S070C.h` – Pins 39‑42 (HSYNC, VSYNC, DE, PCLK) müssen korrekt sein. |
| Touch funktioniert nicht | I2C‑Adresse falsch / Kabelbruch | Führe I2C‑Scanner aus (Adresse 0x5D für GT911). |
| Zähler zählt nicht | Sensor‑Pin falsch oder falsche Polarität | Überprüfe `PIN_SENSOR_IN` und `SENSOR_ACTIVE_LOW` (PC817 = active LOW, Pullup am GPIO). |
| Upload hängt | Bootloader nicht erreicht | BOOT‑Taste während des Uploads drücken (siehe oben). |
| Motor schaltet nicht | Relais‑Modul benötigt HIGH? | Setze `MOTOR_ACTIVE_HIGH` entsprechend. |

---

## 🔗 Nützliche Links

- **LovyanGFX**: https://github.com/lovyan03/LovyanGFX  
- **LVGL**: https://github.com/lvgl/lvgl  
- **Beispiel‑Projekt (Sunton)**: https://github.com/HarryVienna/Makerfabs-Sunton-ESP32-S3-7-Display-with-LovyanGFX-and-LVGL  
- **ESP32‑S3 Technische Referenz**: https://www.espressif.com  
- **Weiterführende Literatur**:  
  *Automation für die Bundesliga mit n8n* – Amir Mobasheraghdam  
  https://buchshop.bod.de/ergebnis-automation-fuer-die-bundesliga-mit-n8n-amir-mobasheraghdam-9783695724925  
  https://amzn.eu/d/0aAtg00j

---

## 📞 Support

Bei Problemen: Amir Mobasheraghdam
1. Seriellen Monitor (115200 Baud) öffnen und Fehlermeldungen dokumentieren.
2. Pin‑Belegung mit der tatsächlichen Verdrahtung vergleichen.
3. Issue auf GitHub erstellen oder ESP32‑Forum konsultieren.

**Viel Erfolg mit Ihrem Bandzähler!** 🚀
```
