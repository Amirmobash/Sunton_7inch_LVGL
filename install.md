# VS Code + PlatformIO Setup (ESP32-S3 Sunton 8048S070 / LVGL + LovyanGFX)

## Voraussetzungen

Du brauchst genau diese Komponenten:

### 1) Visual Studio Code

* Installiere **Visual Studio Code** (Stable)

### 2) PlatformIO IDE (VS Code Extension)

* VS Code → **Extensions**
* Suche: **PlatformIO IDE**
* **Install**

> PlatformIO übernimmt Build/Upload/Library-Management.

### 3) USB/COM-Treiber (Windows)

Damit dein Board als **COM-Port** erscheint:

* Öffne: **Geräte-Manager** → **Anschlüsse (COM & LPT)**
* Wenn dort **COMx** erscheint → ✅ OK
* Wenn **kein COM** erscheint → installiere je nach USB-Chip:

**Typische Treiber:**

* **CP210x (Silicon Labs)**
* **CH340/CH341**
* (Manchmal) **ESP32-S3 USB-CDC/JTAG** funktioniert ohne Extra-Treiber

> Ziel: Du musst am Ende im Geräte-Manager **COM4/COMx** sehen.

### 4) Git (empfohlen)

Für GitHub-Repos/Updates:

* **Git for Windows** installieren

### 5) Python

* **Nicht nötig**, PlatformIO bringt seine Python-Umgebung mit.

---

## Projekt anlegen (PlatformIO)

1. VS Code öffnen
2. Links unten **PlatformIO** (Alien-Icon) → **PIO Home**
3. **New Project**

   * **Board:** `sunton_s3`
   * **Framework:** `Arduino`
4. Projekt erstellen lassen

---

## `platformio.ini` (Copy-Paste)

Passe nur **COM-Port** an.

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

---

## Ordnerstruktur (Empfehlung)

So ist es am stabilsten:

```
include/
  LGFX_Sunton_8048S070C.h

src/
  main.cpp
  lv_conf.h
```

> Wichtig: `lv_conf.h` liegt in `src/`, weil du `-I./src` + `LV_CONF_INCLUDE_SIMPLE` nutzt.

---

## Build & Upload

### In VS Code

* PlatformIO → **Project Tasks** → `sunton_s3`

  * **Build**
  * **Upload**
  * **Monitor**

### Oder im Terminal

```bash
pio run
pio run -t upload
pio device monitor -b 115200
```

---

## Häufige Probleme

### A) Kein COM-Port sichtbar

* Kabel tauschen (Datenkabel!)
* Anderer USB-Port
* Treiber installieren (CP210x oder CH340)
* Danach VS Code neu starten

### B) `lv_conf.h not found`

* Prüfe: `src/lv_conf.h` existiert wirklich
* `platformio.ini` enthält:

  * `-DLV_CONF_INCLUDE_SIMPLE`
  * `-I./src`
* Danach: **Clean** + Build

### C) Upload klappt nicht

* BOOT gedrückt halten → einmal Reset → BOOT loslassen → nochmal Upload
* COM-Port korrekt?

---

## Links (nützlich)

* PlatformIO Board Doc: `sunton_s3`
* LovyanGFX: GitHub `lovyan03/LovyanGFX`
* LVGL: GitHub `lvgl/lvgl`

(Wenn du willst, sage ich dir die exakten Suchbegriffe, damit du die Seiten sofort findest.)

---
