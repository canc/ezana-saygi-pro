# Changelog

## 1.0.0 — 2026-08-31

- Initial native Win32 application `AdhanVolume.exe` (Windows 7–11, static MinGW build).
- Master-volume fade around Fajr, Dhuhr, Asr, Maghrib, and Isha (Sunrise excluded).
- Default location Antalya, Türkiye; timezone Europe/Istanbul (GMT+3).
- Aladhan HTTPS provider with Islamic Finder–compatible fallback.
- Daily per-location cache; API is not polled. Daily cache check at 03:10 Europe/Istanbul.
- Tray-resident UI, hide-on-close, no startup registration, no admin elevation.
- Crash/resume recovery for in-progress volume events.
