## 📋 **Technische Daten**
- **Mikrocontroller**: ESP32-S3 (240MHz, 16MB Flash, 8MB PSRAM)
- **Display**: 7 Zoll TN-Panel, 800×480 Pixel, RGB565-Schnittstelle
- **Touch**: Kapazitiv, 5-Punkt, GT911 Controller
- **Anschlüsse**: USB-C, SD-Kartensteckplatz, I2S Audio-Ausgang
- **Besonderheit**: Parallele RGB-Schnittstelle (kein SPI)

---

## 🚀 **Schnellstart mit PlatformIO**

### **1. Projekt einrichten**
```ini
; platformio.ini
[env:sunton_esp32_s3]
platform = espressif32@6.3.1
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200

; Wichtige Compiler-Flags
build_flags = 
    -DBOARD_HAS_PSRAM
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DARDUINO_USB_MODE=1
    -DCORE_DEBUG_LEVEL=3

; Bibliotheken
lib_deps = 
    lovyan03/LovyanGFX@^1.1.7
    lvgl/lvgl@^8.3.7
```

### **2. Hauptprogramm (main.cpp)**
```cpp
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <lvgl.h>
#include <Wire.h>

// Display-Konfiguration für Jingcai/Sunton
class LGFX : public lgfx::LGFX_Device {
public:
    lgfx::Bus_RGB   bus;
    lgfx::Panel_RGB panel;
    lgfx::Light_PWM light;
    lgfx::Touch_GT911 touch;

    LGFX() {
        // Panel-Einstellungen
        {
            auto cfg = panel.config();
            cfg.panel_width   = 800;
            cfg.panel_height  = 480;
            cfg.memory_width  = 800;
            cfg.memory_height = 480;
            panel.config(cfg);
        }

        // RGB-Bus-Einstellungen (WICHTIG!)
        {
            auto cfg = bus.config();
            // Daten-Pins
            cfg.pin_d0  = GPIO_NUM_15;  // B0
            cfg.pin_d1  = GPIO_NUM_7;   // B1
            cfg.pin_d2  = GPIO_NUM_6;   // B2
            cfg.pin_d3  = GPIO_NUM_5;   // B3
            cfg.pin_d4  = GPIO_NUM_4;   // B4
            cfg.pin_d5  = GPIO_NUM_9;   // G0
            cfg.pin_d6  = GPIO_NUM_46;  // G1
            cfg.pin_d7  = GPIO_NUM_3;   // G2
            cfg.pin_d8  = GPIO_NUM_8;   // G3
            cfg.pin_d9  = GPIO_NUM_16;  // G4
            cfg.pin_d10 = GPIO_NUM_1;   // G5
            cfg.pin_d11 = GPIO_NUM_14;  // R0
            cfg.pin_d12 = GPIO_NUM_21;  // R1
            cfg.pin_d13 = GPIO_NUM_47;  // R2
            cfg.pin_d14 = GPIO_NUM_48;  // R3
            cfg.pin_d15 = GPIO_NUM_45;  // R4

            // Steuer-Pins
            cfg.pin_henable = GPIO_NUM_41;  // DE
            cfg.pin_vsync   = GPIO_NUM_40;  // VSYNC
            cfg.pin_hsync   = GPIO_NUM_39;  // HSYNC
            cfg.pin_pclk    = GPIO_NUM_42;  // PCLK
            
            // Timing (optimiert für 800x480)
            cfg.freq_write = 12000000;  // 12MHz
            
            cfg.hsync_polarity    = 0;
            cfg.hsync_front_porch = 8;
            cfg.hsync_pulse_width = 2;
            cfg.hsync_back_porch  = 43;
            
            cfg.vsync_polarity    = 0;
            cfg.vsync_front_porch = 8;
            cfg.vsync_pulse_width = 2;
            cfg.vsync_back_porch  = 12;
            
            cfg.pclk_idle_high    = 1;
            bus.config(cfg);
            panel.setBus(&bus);
        }

        // Hintergrundbeleuchtung
        {
            auto cfg = light.config();
            cfg.pin_bl = GPIO_NUM_2;  // Backlight-Pin
            light.config(cfg);
            panel.light(&light);
        }

        // Touch-Controller
        {
            auto cfg = touch.config();
            cfg.i2c_port = I2C_NUM_0;
            cfg.pin_sda  = GPIO_NUM_19;
            cfg.pin_scl  = GPIO_NUM_20;
            cfg.pin_rst  = GPIO_NUM_38;  // Reset-Pin
            cfg.x_max    = 800;
            cfg.y_max    = 480;
            touch.config(cfg);
            panel.setTouch(&touch);
        }

        setPanel(&panel);
    }
};

// Globale Instanzen
static LGFX tft;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[800 * 10];
static lv_color_t buf2[800 * 10];

// LVGL Display-Funktion
void display_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.writePixels((lgfx::rgb565_t*)color_p, w * h);
    tft.endWrite();

    lv_disp_flush_ready(disp);
}

// Touch-Read-Funktion
void touch_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    uint16_t touchX, touchY;
    bool touched = tft.getTouch(&touchX, &touchY);

    if(touched) {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = touchX;
        data->point.y = touchY;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("Starte Jingcai ESP32-8048S070C...");

    // I2C für Touch initialisieren
    Wire.begin(19, 20);
    Wire.setClock(400000);

    // Display initialisieren
    tft.init();
    tft.setBrightness(128);
    tft.setRotation(0);

    // LVGL initialisieren
    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, 800 * 10);

    // Display-Treiber registrieren
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 800;
    disp_drv.ver_res = 480;
    disp_drv.flush_cb = display_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // Touch-Treiber registrieren
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read;
    lv_indev_drv_register(&indev_drv);

    // Einfache UI erstellen
    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "Jingcai ESP32-8048S070C\nDisplay-Test erfolgreich!");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    
    Serial.println("System bereit!");
}

void loop() {
    lv_timer_handler();
    delay(5);
}
```

---

## 🔧 **Wichtige Einstellungen für Arduino IDE**

### **Board-Konfiguration:**
```
Board: "ESP32S3 Dev Module"
USB CDC On Boot: Enabled
CPU Frequency: 240MHz
Flash Size: 16MB
PSRAM: OPI PSRAM
Partition Scheme: 16M Flash (3M APP/9.9M FATFS)
Upload Speed: 921600
```

### **Bibliotheken installieren Amir Mobasher:**
1. **LovyanGFX** (Version 1.1.13+)
2. **LVGL** (Version 8.3.7+)
3. **Wire** (bereits installiert)

---

## ⚠️ **Häufige Probleme und Lösungen**

### **1. Display bleibt schwarz, aber Backlight leuchtet**
**Problem**: RGB-Pins falsch konfiguriert  
**Lösung**: Pins 39-42 (HSYNC, VSYNC, DE, PCLK) überprüfen

### **2. Touch funktioniert nicht**
```cpp
// I2C-Scanner ausführen
#include <Wire.h>
void setup() {
    Wire.begin(19, 20);
    Serial.begin(115200);
    for(uint8_t addr=1; addr<127; addr++) {
        Wire.beginTransmission(addr);
        if(Wire.endTransmission()==0) {
            Serial.print("Gefunden: 0x");
            Serial.println(addr, HEX);
        }
    }
}
```

### **3. Upload hängt**
**Bootloader-Modus aktivieren:**
1. BOOT-Taste gedrückt halten
2. RST-Taste kurz drücken
3. BOOT-Taste loslassen
4. Upload starten

---

## 📊 **Pintabelle (Jingcai ESP32-8048S070C)**

| Funktion | Pin | Alternative |
|----------|-----|-------------|
| Backlight | GPIO 2 | GPIO 45 |
| HSYNC | GPIO 39 | - |
| VSYNC | GPIO 40 | - |
| DE | GPIO 41 | - |
| PCLK | GPIO 42 | - |
| Touch SDA | GPIO 19 | - |
| Touch SCL | GPIO 20 | - |
| Touch Reset | GPIO 38 | - |

---

## 🔗 **Nützliche Links**

### **Offizielle Repositories:**
1. **LovyanGFX**: https://github.com/lovyan03/LovyanGFX
2. **LVGL**: https://github.com/lvgl/lvgl
3. **Beispielprojekt**: https://github.com/HarryVienna/Makerfabs-Sunton-ESP32-S3-7-Display-with-LovyanGFX-and-LVGL

### **Dokumentation:**
- LVGL Docs: https://docs.lvgl.io
- ESP32-S3 Technical Reference: https://www.espressif.com

### **Kaufquellen:**
- Tinytronics: https://www.tinytronics.nl
- Makerfabs: https://www.makerfabs.com

---

## 🎯 **Erster Test (Einfarbiger Bildschirm)**

```cpp
// Minimaler Test ohne LVGL
#include <LovyanGFX.hpp>
LGFX tft;

void setup() {
    // Backlight einschalten
    pinMode(2, OUTPUT);
    digitalWrite(2, HIGH);
    
    // Display initialisieren
    tft.init();
    
    // Roter Bildschirm (10 Sekunden)
    tft.fillScreen(TFT_RED);
    delay(10000);
    
    // Grüner Bildschirm
    tft.fillScreen(TFT_GREEN);
    delay(5000);
    
    // Text anzeigen
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(3);
    tft.setCursor(100, 200);
    tft.println("TEST OK!");
}

void loop() {}
```

---

## 📞 **Support**

Bei Problemen:
1. **Serial Monitor** öffnen (115200 Baud)
2. Fehlermeldungen dokumentieren
3. GitHub Issue erstellen
4. ESP32 Forum konsultieren

**Viel Erfolg mit Ihrem Jingcai ESP32-8048S070C Display!** 🚀
