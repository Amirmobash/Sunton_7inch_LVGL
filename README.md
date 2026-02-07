# Sunton/Jingcai ESP32-8048S070C — LVGL + LovyanGFX (PlatformIO)

این پروژه برای بردهای زیر آماده است:
- Jingcai ESP32-8048S070C
- Sunton ESP32-8048S070 (همان مدل با برند متفاوت)

مشخصات: ESP32-S3, LCD 7" 800x480 RGB565, Touch GT911

## نیازمندی‌ها
- VS Code
- افزونه PlatformIO IDE

## اجرا
1) پوشه پروژه را در VS Code باز کن (File → Open Folder)
2) Build (علامت ✔️ در نوار پایین)  
3) Upload (فلش ➡️)
4) Monitor (برای دیدن لاگ سریال)

## اگر Upload گیر کرد
روی بعضی ESP32-S3 ها باید وارد Bootloader بشی:
- BOOT را نگه دار
- یک بار RST/EN را بزن
- BOOT را ول کن
- دوباره Upload

## تنظیم COM در ویندوز
اگر PlatformIO پورت را خودکار پیدا نکرد:
- Device Manager → Ports → شماره COM را ببین
- در platformio.ini خطوط upload_port و monitor_port را فعال کن و COM را بزن.
