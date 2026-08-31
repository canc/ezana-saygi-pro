# Changelog

## 1.0.1 — 2026-08-31

- Turkish as the default UI language (window, buttons, tray, errors, notifications).
- Tray menu reduced to **Göster** and **Kapat**.
- Close (X) hides to tray with no confirmation; only **Kapat** exits.
- Double-click tray icon shows and focuses the main window.
- Release binary committed at `dist/AdhanVolume.exe`.

## 1.0.0 — 2026-08-31

- Initial native Win32 application `AdhanVolume.exe` (Windows 7–11, static MinGW build).
- Master-volume fade around Fajr, Dhuhr, Asr, Maghrib, and Isha (Sunrise excluded).
- Default location Antalya, Türkiye; timezone Europe/Istanbul (GMT+3).
- Aladhan HTTPS provider with Islamic Finder–compatible fallback.
- Daily per-location cache; API is not polled. Daily cache check at 03:10 Europe/Istanbul.
- Tray-resident UI, hide-on-close, no startup registration, no admin elevation.
- Crash/resume recovery for in-progress volume events.
