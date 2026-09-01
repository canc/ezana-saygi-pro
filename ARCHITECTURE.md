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
- Master volume: WASAPI `IAudioEndpointVolume` on the **current** default render endpoint (`eConsole`, then `eMultimedia`), with WinMM mixer fallback. Endpoint is re-resolved on failure, default-device change (`IMMNotificationClient`), and each fade capture. COM is initialized once on the UI thread (`CoInitializeEx` apartment).
- HTTPS: WinHTTP with TLS 1.0–1.2 enabled (`WINHTTP_OPTION_SECURE_PROTOCOLS`)
- Paths: `SHGetFolderPath` (`CSIDL_APPDATA`)
- COM: `CoInitializeEx` apartment-threaded on the UI thread

Windows 11-only APIs are not used. DPI awareness is loaded dynamically (`SetProcessDPIAware`).

## Module map

```
Application (Win32 UI thread + one HTTP worker)
│
├── UI
│   ├── Main window (status, location, timezone, threshold, next prayer, enable/disable action)
│   └── TrayManager (hide-on-close, Göster / Kapat)
│
├── Core
│   ├── TimezoneService     Europe/Istanbul = GMT+3 (not Windows TZ)
│   ├── PrayerScheduler     adaptive wall-clock poll; WAITING_FOR_THRESHOLD is independent of the UI
│   ├── FadeController      linear interpolation
│   ├── VolumeController    WASAPI IAudioEndpointVolume (default render); WinMM mixer fallback
│   └── App config          JSON in %APPDATA%\AdhanVolume
│
├── Infrastructure
│   ├── PrayerTimeProvider
│   │     ├── AladhanProvider          primary (Diyanet method 13 for Turkey)
│   │     └── IslamicFinderProvider    fallback (JSON API if configured, else public city page)
│   ├── CacheManager / repository      ONLY layer that may call a provider
│   ├── HttpClient (WinHTTP)
│   └── Logger (rotated)
│
└── Resources
    └── muted-speaker icon (16/32/48)
```

The scheduler **never** calls the HTTP client. It reads an in-memory schedule filled by `CacheManager`.

## Prayer times

`PrayerTimeProvider` is an interface. `FallbackProvider` tries **Aladhan first**, then IslamicFinder.org.

```
Aladhan timingsByCity
  city, country, method=13 (Diyanet İşleri Başkanlığı) for Turkey
        ↓ fail
IslamicFinder JSON API (optional, never /index.php/api/prayer_times)
        ↓ fail / not configured
IslamicFinder.org public city page (one GET)
        ↓ fail
No schedule (no fabricated times)
```

`AladhanProvider` stores `calculation_method` (default **13**) and always serializes it on the request. For Isparta the URL contains `city=Isparta&country=Turkey&method=13`. Saved `.../v1/timings` endpoints are rewritten to `timingsByCity`.

`IslamicFinderProvider` composes `IslamicFinderApiClient` and `IslamicFinderPageClient` and remains the fallback only. HTML parsing stays in the provider; the scheduler and cache manager never scrape pages.

The page client prefers embedded JSON when it contains timings, then the `#monthly-prayers` table (matched to the requested Europe/Istanbul date), then labeled meta / semantic tiles. Sunrise, sunset, and Qiyam are ignored as volume events.

Cache files store `source` (`aladhan` / `islamicfinder`), `calculation_method`, and `provider_config_version`. Version **2** is Aladhan-primary + Diyanet 13; older IslamicFinder-primary files are discarded and refetched. A valid current-version cache is reused with zero network requests.

Responses are validated (required prayers İmsak/Fajr, Öğle, İkindi, Akşam, Yatsı; HH:MM; monotonic times; date). Failures keep the last good **current-version** cache. Nothing is invented locally. Volume automation does not run without a valid schedule.

## Cache identity and 03:10 Europe/Istanbul

Cache key:

```
{timezone}:{lat:.4f}:{lon:.4f}:{YYYY-MM-DD}
```

Example: `Europe/Istanbul:36.8969:30.6966:2026-08-31`

The **date** and the daily **check** are computed in **Europe/Istanbul (GMT+3)** from UTC timestamps. The Windows zone, hosting zone, and API server zone are ignored.

At 03:10 Istanbul the app **validates** today’s cache. It contacts IslamicFinder (then Aladhan only if needed) only if that entry is missing or invalid.

On startup, resume, or location change it also checks immediately — it does not wait until the next 03:10.

Retention: about 14 days of files; other locations are not deleted just because the user switched city.

## Scheduler state

```
IDLE → WAITING_FOR_THRESHOLD → FADING_OUT → MUTED → FADING_IN → RESTORED → IDLE
```

Internal times are UTC unix timestamps. Display and cache dates are converted with the location zone (Istanbul offset for the default).

The threshold is the **start** of fade-out, not the moment volume reaches 0.

The scheduler consumes the structured `PrayerSchedule` (unix timestamps in the location zone). It does **not** parse “Sonraki ezan” text. Before fade-out it holds an explicit `WAITING_FOR_THRESHOLD` event so threshold detection is independent of the next-prayer label.

Event id `date + prayer + location` plus a processed-id set prevent duplicates. If the wall clock jumps **backward** into a window that was already marked processed, that id is un-marked so the event can run again (manual clock tests). Jumps **forward** into the window start fade immediately; jumps past `fade_in_end` skip the event.

The loop is `SetTimer` with an adaptive interval: about **1 second** when the next fade start is within 2 minutes, **5 seconds** within 10 minutes, **15 seconds** when idle, and **100 ms** only while fading. There is no one-shot “sleep until 19:22”. `WM_TIMECHANGE` and resume also force an immediate reevaluation using `GetSystemTimeAsFileTime` (not `GetTickCount`).

`active_event.json` records a captured original volume so a crash/restart can restore it when the window has ended, without overriding a volume the user already changed.

## Volume policy

- Capture original scalar before the first change for that event.
- Fade to 0; hold 0 during the Adhan window (re-apply 0 if the user raises volume).
- Fade back to the captured value.
- Never toggle mute. If the system was muted, it stays muted.
- If original volume was 0, skip the animation and stay at 0.
- Disable, location change, or Exit restore the captured volume immediately.

## Tests

`tests/test_main.cpp` runs on the host (Linux) against the same core library: timezone/03:10 math, cache hit/miss, location switch, API failure, scheduler fade, mute-hold, clock-jump forward/back, volume self-test, sleep/wake style resume. `make test-tz` repeats the suite under several `TZ` values to prove cache dates do not use `localtime()`.
