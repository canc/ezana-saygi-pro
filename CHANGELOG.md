# Changelog

## 1.0.3 — 2026-08-31

- Visible product name is **Ezana Saygı PRO** (bold in the main window).
- Per-prayer Adhan durations with defaults: İmsak 5 dk, Öğle 5 dk, İkindi 3 dk, Akşam 3 dk, Yatsı 7 dk. **Ezan Sürelerini Ayarla** opens a popup (İptal / Kaydet).
- Threshold choices are 30 saniye, 1 dakika, and 2 dakika.
- Existing `adhan_duration_seconds` configs migrate; new files store `adhan_durations` only.

## 1.0.2 — 2026-08-31

- Scheduler no longer sleeps until the next prayer: it reevaluates wall-clock time on an adaptive poll (about 1s when an event is near, 5s when soon, 15s when idle) and on `WM_TIMECHANGE`.
- Manual system-clock jumps (forward or backward) rebuild event state. A processed prayer is un-marked if its Adhan window is still open, so clock-back tests can retrigger fade.
- Volume events are selected from the structured schedule, not from the “Sonraki ezan” UI string. The scheduler enters `WAITING_FOR_THRESHOLD` before fade-out starts.
- WASAPI `IAudioEndpointVolume` rebinds the current default render endpoint on failure, device change, and each capture. Set operations are verified by reading the volume back. HRESULT errors are logged. WinMM mixer remains a fallback.
- `--volume-test` runs a get/set/verify/restore probe independent of the scheduler. `--debug` enables verbose scheduler ticks.
- Release artifacts: `AdhanVolume.exe` / `AdhanVolume-x64.exe` (Windows 7–11 x64), `AdhanVolume-x86.exe` (Win7 32-bit and Windows 10 ARM x86 emulation), `AdhanVolume-arm64.exe` (native Windows 10/11 ARM64).

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
