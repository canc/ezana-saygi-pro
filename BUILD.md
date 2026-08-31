# Building Adhan Volume

The release artifact is a **standalone** `AdhanVolume.exe`. Users do not install a compiler, runtime, or redistributable.

Windows 10 is the primary target. The same binary is intended for Windows 7, 8, 8.1, and 11 (x64). A 32-bit build is optional for older 32-bit Windows 7 PCs.

## Development tools

### Cross-compile from Linux (this repository’s default)

- `g++` (C++11) — unit tests
- `make`
- `python3` — generates `assets/app.ico` at build time
- `mingw-w64` — Windows cross compiler

Debian/Ubuntu:

```bash
sudo apt-get install -y g++ make python3 mingw-w64
```

### Build on Windows

Either:

- MinGW-w64 (MSYS2 `mingw-w64-x86_64-gcc`) plus `make` and `python`, or
- Visual Studio 2017+ with the C++ desktop workload (Windows 7 compatibility: do not require a newer Universal CRT redistributable; use static CRT)

## Commands

From the repository root:

```bash
make test          # native unit tests (Linux/macOS)
make test-tz       # same tests under TZ=UTC, America/New_York, Europe/London
make windows       # cross-compile dist/AdhanVolume.exe (x64)
make windows-x86   # optional 32-bit exe
make clean
```

`make windows` produces:

- `build/AdhanVolume.exe`
- `dist/AdhanVolume.exe`  ← copy this file to a Windows machine

The executable is linked with `-static -static-libgcc -static-libstdc++ -mwindows`. It uses only OS DLLs that ship with Windows 7+ (`winhttp.dll`, `ole32.dll`, `user32.dll`, …).

## How the standalone EXE is produced

1. `python3 assets/generate_icon.py` writes a 16/32/48 muted-speaker ICO.
2. `windres` embeds the icon, version info, and an asInvoker manifest (no UAC).
3. `x86_64-w64-mingw32-g++` compiles `src/core/*` and `src/win/*` into one PE file.

There is no installer, no side-by-side assembly besides Common Controls v6 (already on Windows 7+), and no .NET host.

## MSVC (Windows) sketch

```bat
rc /fo app.res src\win\app.rc
cl /nologo /O2 /EHsc /DUNICODE /D_UNICODE /DWINVER=0x0601 /D_WIN32_WINNT=0x0601 ^
   /I src /I src\win /MT /FeAdhanVolume.exe ^
   src\core\*.cpp src\win\*.cpp app.res ^
   winhttp.lib ole32.lib uuid.lib comctl32.lib shell32.lib user32.lib gdi32.lib ^
   winmm.lib advapi32.lib shlwapi.lib oleaut32.lib
```

`/MT` static CRT avoids the Visual C++ Redistributable.

## Release checks

On a **clean Windows 10 VM** with no Visual Studio, Python, or .NET SDK:

1. Copy only `AdhanVolume.exe`.
2. Run it. The window and tray icon appear.
3. Confirm `%APPDATA%\AdhanVolume\config.json` is created.
4. Confirm prayer times appear (or a cache/error hint if offline).
5. Close the window: process remains in the tray. Exit from the tray menu.

Windows 7: TLS 1.2 and current trusted roots are required for HTTPS. Fully updated Windows 7 SP1 is expected.

## Supported architectures

| Build                    | Arch | Typical OS          |
|--------------------------|------|---------------------|
| `AdhanVolume.exe`        | x64  | Win7–11 64-bit      |
| `AdhanVolume-x86.exe`    | x86  | Win7 32-bit (optional) |

ARM64 is not produced; Windows 11 ARM can run the x64 binary through emulation if needed.
