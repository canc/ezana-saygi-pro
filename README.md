# Adhan Volume

Lightweight native Windows utility that fades the **system master volume** to 0 before each prayer (Adhan) time, holds it at 0 during the expected Adhan, then restores the exact volume you had before.

Default location: **Antalya, Türkiye** — timezone **Europe/Istanbul (GMT+3)**.

## Requirements

- Windows 7, 8, 8.1, 10, or 11 (Windows 10 is the primary target)
- No Java, Python, .NET, Node.js, Electron, WebView2, or Visual C++ Redistributable
- Run as a standard user (no Administrator / UAC)

The release file is a single executable: **AdhanVolume.exe**.

## Installation

1. Copy `AdhanVolume.exe` to a folder you control (for example `Documents\AdhanVolume`).
2. Double-click it. No installer.

Settings, cache, and logs are stored in:

`%APPDATA%\AdhanVolume\`

```
config.json
cache\
logs\
```

## First launch

1. Confirm **Location** (default: Antalya, Turkey).
2. Confirm **Timezone** shows `Europe/Istanbul (GMT+3)` for Türkiye.
3. Set **Threshold** (how long before prayer the fade starts): 30 seconds, 1 minute, 3 minutes, or 5 minutes. Default is **1 minute**.
4. Use **ON / OFF** to enable or disable automatic fading. Scheduling only runs while the process is running (including when hidden in the tray).

The app does **not** add itself to Windows startup.

## Location

Pick a city from the list. Latitude/longitude are filled in automatically.

**Detect Location** uses the Windows country setting only (no browser/WebView). It never requires a location permission prompt. You can always choose a city by hand.

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

- **ON / OFF** in the window enables or disables fading.
- Closing the window **hides** it. The app keeps running in the system tray.
- Tray menu: Open, Enable/Disable, Next Prayer, Refresh Prayer Times, Exit.
- Double-click the tray icon to open the window.
- **Exit** is the only way to fully quit. On exit, an in-progress fade is restored so volume is not left at 0.

## Prayer times API and cache

Prayer times are loaded from a public HTTPS Awqat Salah API (Aladhan, Diyanet method for Turkey), with an Islamic Finder–compatible endpoint as fallback.

Daily times are **cached per location and calendar date**. The API is **not** called on a timer and is **not** called by the volume scheduler.

The cache date and the daily check use **Europe/Istanbul**, not the Windows time zone and not the API server’s time zone.

Normal API contact:

1. First launch (or today’s cache missing/invalid)
2. Daily cache **check** at **03:10 Europe/Istanbul** — fetches only if today’s cache is missing
3. Location/timezone change when that location has no cache for today
4. Manual **Refresh Prayer Times**

Restarting the same day with a valid cache performs **zero** API requests.

If the computer sleeps through 03:10, the check runs immediately on wake/start: fetch only if today’s cache is missing.

## Offline behavior

On startup the last valid cache is shown immediately. If the network is down and a valid cache exists, that cache is used. If there is no valid cache, the app stays running and **does not** change volume.

Invalid API responses are discarded. The previous valid cache is kept.

## Troubleshooting

Logs: `%APPDATA%\AdhanVolume\logs\adhanvolume.log` (rotated, size-limited).

| Problem | What to check |
|---------|----------------|
| Volume never fades | App must be **ON**, in the tray (not Exited), and a valid schedule must be loaded. |
| Wrong prayer times | Confirm city and that timezone is Europe/Istanbul for Türkiye. Use Refresh Prayer Times. |
| Windows 7 TLS/HTTPS errors | Install Windows 7 updates that enable TLS 1.2 and modern root certificates. |
| Volume stuck at 0 after a crash | Restart the app once; it restores the captured volume when the unfinished event has ended, unless you already changed the volume yourself. |
| Two copies running | Only one instance is allowed; launching again focuses the existing window. |

The app controls **master** system volume only. It does not mute individual programs, kill players, or change the Windows mute flag (if you were already muted, you stay muted).

## Privacy

No accounts, telemetry, ads, or cloud sync. HTTPS only. No personal data is uploaded. Logs do not include audio content.

## Build

See [BUILD.md](BUILD.md) and [ARCHITECTURE.md](ARCHITECTURE.md).
