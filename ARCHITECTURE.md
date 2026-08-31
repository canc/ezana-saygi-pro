# Architecture

Adhan Volume is a small **native Win32 C++11** application. That choice is what makes Windows 7–11, a single `.exe`, and Core Audio / tray / WinHTTP possible without Java, Python, .NET, Node, Electron, or WebView2.

## Why this stack

| Option | Rejected because |
|--------|------------------|
| Electron / Node | Huge runtime, no Win7 story we want, extra installer |
| .NET (even self-contained) | Large payload; Framework install forbidden by the spec |
| Python / Java | External runtime |
| Rust/Go recent toolchains | Windows 7 dropped or painful |
| Native C++ / Win32 + MinGW static link | Fits every distribution and OS constraint |

APIs used (all present on Windows 7):

- Window + tray: `user32`, `shell32` (`Shell_NotifyIcon`)
- Master volume: WASAPI `IAudioEndpointVolume` (Vista+), mixer API fallback
- HTTPS: WinHTTP with TLS 1.0–1.2 enabled (`WINHTTP_OPTION_SECURE_PROTOCOLS`)
- Paths: `SHGetFolderPath` (`CSIDL_APPDATA`)
- COM: `CoInitializeEx` apartment-threaded on the UI thread

Windows 11-only APIs are not used. DPI awareness is loaded dynamically (`SetProcessDPIAware`).

## Module map

```
Application (Win32 UI thread + one HTTP worker)
│
├── UI
│   ├── Main window (status, location, timezone, threshold, next prayer, ON/OFF)
│   └── TrayManager (hide-on-close, Göster / Kapat)
│
├── Core
│   ├── TimezoneService     Europe/Istanbul = GMT+3 (not Windows TZ)
│   ├── PrayerScheduler     20s idle poll, 100ms only while fading
│   ├── FadeController      linear interpolation
│   ├── VolumeController    master scalar only; mute flag preserved
│   └── App config          JSON in %APPDATA%\AdhanVolume
│
├── Infrastructure
│   ├── PrayerTimeProvider
│   │     ├── AladhanProvider          public Awqat Salah HTTPS API (default)
│   │     └── IslamicFinderProvider    compatible fallback endpoint
│   ├── CacheManager / repository      ONLY layer that may call the API
│   ├── HttpClient (WinHTTP)
│   └── Logger (rotated)
│
└── Resources
    └── muted-speaker icon (16/32/48)
```

The scheduler **never** calls the HTTP client. It reads an in-memory schedule filled by `CacheManager`.

## Prayer times

`PrayerTimeProvider` is an interface. `FallbackProvider` tries Aladhan first (no API key, HTTPS, Diyanet method 13 for Turkey), then the configured Islamic Finder–compatible URL.

Responses are validated (required prayers, HH:MM, monotonic times, date). Failures keep the last good cache. Nothing is invented locally.

## Cache identity and 03:10 Europe/Istanbul

Cache key:

```
{timezone}:{lat:.4f}:{lon:.4f}:{YYYY-MM-DD}
```

Example: `Europe/Istanbul:36.8969:30.6966:2026-08-31`

The **date** and the daily **check** are computed in **Europe/Istanbul (GMT+3)** from UTC timestamps. The Windows zone, hosting zone, and API server zone are ignored.

At 03:10 Istanbul the app **validates** today’s cache. It requests the API only if that entry is missing or invalid.

On startup, resume, or location change it also checks immediately — it does not wait until the next 03:10.

Retention: about 14 days of files; other locations are not deleted just because the user switched city.

## Scheduler state

```
IDLE → WAITING_FOR_THRESHOLD → FADING_OUT → MUTED → FADING_IN → RESTORED → IDLE
```

Internal times are UTC unix timestamps. Display and cache dates are converted with the location zone (Istanbul offset for the default).

The threshold is the **start** of fade-out, not the moment volume reaches 0.

Event id `date + prayer + location` plus a processed-id set prevent duplicates across refresh, sleep, and clock changes. The loop is `SetTimer` 20s (idle), not a busy wait.

`active_event.json` records a captured original volume so a crash/restart can restore it when the window has ended, without overriding a volume the user already changed.

## Volume policy

- Capture original scalar before the first change for that event.
- Fade to 0; hold 0 during the Adhan window (re-apply 0 if the user raises volume).
- Fade back to the captured value.
- Never toggle mute. If the system was muted, it stays muted.
- If original volume was 0, skip the animation and stay at 0.
- Disable, location change, or Exit restore the captured volume immediately.

## Tests

`tests/test_main.cpp` runs on the host (Linux) against the same core library: timezone/03:10 math, cache hit/miss, location switch, API failure, scheduler fade, mute-hold, sleep/wake style resume. `make test-tz` repeats the suite under several `TZ` values to prove cache dates do not use `localtime()`.
