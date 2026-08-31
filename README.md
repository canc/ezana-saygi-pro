# Adhan Volume

Lightweight native Windows utility that fades the **system master volume** to 0 before each prayer (Adhan) time, holds it at 0 during the expected Adhan, then restores the exact volume you had before.

Default location: **Antalya, Türkiye** — timezone **Europe/Istanbul (GMT+3)**.

## Requirements

- Windows 7, 8, 8.1, 10, or 11 (Windows 10 is the primary target)
- No Java, Python, .NET, Node.js, Electron, WebView2, or Visual C++ Redistributable
- Run as a standard user (no Administrator / UAC)

The release file is a single executable. On 64-bit Windows 10/11/7 use **AdhanVolume.exe** (same as **AdhanVolume-x64.exe**). On **Windows 10 ARM** use **AdhanVolume-arm64.exe** (native) or **AdhanVolume-x86.exe** (x86 emulation). The x64 EXE does not run on Windows 10 ARM.

## Installation

1. Copy the EXE for your PC to a folder you control (for example `Documents\AdhanVolume`).
   - 64-bit Intel/AMD: `AdhanVolume.exe` or `AdhanVolume-x64.exe`
   - 32-bit Windows 7: `AdhanVolume-x86.exe`
   - Windows 10/11 ARM: `AdhanVolume-arm64.exe` (or `AdhanVolume-x86.exe` on Windows 10 ARM)
2. Double-click it. No installer.

Settings, cache, and logs are stored in:

`%APPDATA%\AdhanVolume\`

```
config.json
cache\
logs\
```

## First launch

The window is in **Turkish** by default.

1. Confirm **Konum** (default: Antalya, Türkiye).
2. Confirm **Zaman Dilimi** shows `Avrupa/İstanbul (GMT+3)`.
3. Set **Ezan Vaktinden Önce**: 30 saniye, 1 dakika, 3 dakika, or 5 dakika. Default is **1 dakika**.
4. Use **Aktif / Pasif** to enable or disable automatic fading. Scheduling continues while the process is in the tray.

The app does **not** add itself to Windows startup.

## Location

Pick a city from the list. Latitude/longitude are filled in automatically.

**Konumu Bul** uses the Windows country setting only (no browser/WebView). It never requires a location permission prompt. You can always choose a city by hand.

## Threshold and Adhan window

Example with Maghrib at 19:23, threshold 1 minute, Adhan duration 3 minutes, fade ~4 seconds:

| Time     | Volume |
|----------|--------|
| 19:21    | your current volume (e.g. 65%) |
| 19:22    | fade-out **starts** |
| 19:22:04 | 0% |
| 19:23    | prayer time, volume stays 0% |
| 19:26    | fade-in starts |
| 19:26:04 | original volume restored |

Sunrise is **not** a trigger. Only Fajr, Dhuhr, Asr, Maghrib, and Isha.

The Adhan hold duration is an internal setting (`adhan_duration_seconds`, default **180**).

## Enable / disable and tray

- **Aktif / Pasif** in the window enables or disables fading.
- Closing the window (X) **hides** it. The app keeps running in the system tray. No confirmation dialog.
- Tray menu is only **Göster** (show window) and **Kapat** (quit).
- Double-click the tray icon to show and focus the window (same as Göster).
- **Kapat** is the only way to fully quit. On exit, an in-progress fade is restored so volume is not left at 0.
- **Vakitleri Yenile** in the main window fetches times again.

## Prayer times API and cache

Prayer times are loaded from a public HTTPS Awqat Salah API (Aladhan, Diyanet method for Turkey), with an Islamic Finder–compatible endpoint as fallback.

Daily times are **cached per location and calendar date**. The API is **not** called on a timer and is **not** called by the volume scheduler.

The cache date and the daily check use **Europe/Istanbul**, not the Windows time zone and not the API server’s time zone.

Normal API contact:

1. First launch (or today’s cache missing/invalid)
2. Daily cache **check** at **03:10 Europe/Istanbul** — fetches only if today’s cache is missing
3. Location/timezone change when that location has no cache for today
4. Manual **Vakitleri Yenile** in the main window

Restarting the same day with a valid cache performs **zero** API requests.

If the computer sleeps through 03:10, the check runs immediately on wake/start: fetch only if today’s cache is missing.

## Offline behavior

On startup the last valid cache is shown immediately. If the network is down and a valid cache exists, that cache is used. If there is no valid cache, the app stays running and **does not** change volume.

Invalid API responses are discarded. The previous valid cache is kept.

## Troubleshooting

Logs: `%APPDATA%\AdhanVolume\logs\adhanvolume.log` (rotated, size-limited).

| Problem | What to check |
|---------|----------------|
| Volume never fades | App must be **Aktif**, in the tray (not closed with **Kapat**), and a valid schedule must be loaded. Logs should show `Threshold reached` then `Starting fade-out` then `Setting master volume`. If the first is missing, the scheduler did not see the window; if fade-out is logged but volume does not change, the audio API failed. |
| Direct audio probe | Run `AdhanVolume.exe --volume-test`. This get/set/verify/restore test ignores prayer times. Results are logged and shown in a dialog. |
| Verbose scheduler log | Run with `--debug` (or set `ADHAN_DEBUG=1`). High-frequency ticks are otherwise limited to the last ~2 minutes before an event. |
| Windows 10 ARM: EXE will not start | Use `AdhanVolume-arm64.exe` or `AdhanVolume-x86.exe`. The x64 build cannot run on Windows 10 ARM. |
| Wrong prayer times | Confirm city and that timezone is Avrupa/İstanbul (GMT+3) for Türkiye. Use **Vakitleri Yenile**. |
| Windows 7 TLS/HTTPS errors | Install Windows 7 updates that enable TLS 1.2 and modern root certificates. |
| Volume stuck at 0 after a crash | Restart the app once; it restores the captured volume when the unfinished event has ended, unless you already changed the volume yourself. |
| Two copies running | Only one instance is allowed; launching again focuses the existing window. |

The app controls **master** system volume only. It does not mute individual programs, kill players, or change the Windows mute flag (if you were already muted, you stay muted).

## Privacy

No accounts, telemetry, ads, or cloud sync. HTTPS only. No personal data is uploaded. Logs do not include audio content.

## Build

See [BUILD.md](BUILD.md) and [ARCHITECTURE.md](ARCHITECTURE.md).
