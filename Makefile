# Adhan Volume — native Windows 7–11 prayer-time volume fader
# Cross-compile from Linux with mingw-w64, or build on Windows with MinGW/MSVC.

.PHONY: all test test-tz windows windows-x86 windows-arm64 windows-all dist icon clean

HOST_CXX ?= g++
HOST_CXXFLAGS ?= -std=c++11 -O2 -Wall -Wextra -I src -pthread

MINGW_PREFIX ?= x86_64-w64-mingw32
MINGW_CXX ?= $(MINGW_PREFIX)-g++
MINGW_WINDRES ?= $(MINGW_PREFIX)-windres
MINGW_CXXFLAGS ?= -std=c++11 -O2 -Wall -Wextra -I src -I src/win \
	-DUNICODE -D_UNICODE -DWINVER=0x0601 -D_WIN32_WINNT=0x0601 -municode
MINGW_LDFLAGS ?= -static -static-libgcc -static-libstdc++ -mwindows
MINGW_LIBS ?= -lwinhttp -lole32 -luuid -lcomctl32 -lshell32 -luser32 -lgdi32 \
	-lwinmm -ladvapi32 -lshlwapi -loleaut32 -lcomdlg32

MINGW32_PREFIX ?= i686-w64-mingw32
MINGW32_CXX ?= $(MINGW32_PREFIX)-g++
MINGW32_WINDRES ?= $(MINGW32_PREFIX)-windres

# Native Windows ARM64 (llvm-mingw). Not required for x64/x86 builds.
LLVM_MINGW ?= $(HOME)/llvm-mingw
ARM64_CXX ?= $(LLVM_MINGW)/bin/aarch64-w64-mingw32-clang++
ARM64_WINDRES ?= $(LLVM_MINGW)/bin/aarch64-w64-mingw32-windres
ARM64_STRIP ?= $(LLVM_MINGW)/bin/aarch64-w64-mingw32-strip

CORE_SRCS := \
	src/core/types.cpp \
	src/core/json.cpp \
	src/core/timezone.cpp \
	src/core/locations.cpp \
	src/core/fsutil.cpp \
	src/core/logger.cpp \
	src/core/config.cpp \
	src/core/schedule.cpp \
	src/core/provider.cpp \
	src/core/cache_manager.cpp \
	src/core/fade.cpp \
	src/core/scheduler.cpp \
	src/core/volume_diag.cpp \
	src/core/i18n.cpp

WIN_SRCS := \
	src/win/win_http.cpp \
	src/win/win_volume.cpp \
	src/win/win_paths.cpp \
	src/win/win_main.cpp

all: test windows

icon: assets/app.ico

assets/app.ico: assets/generate_icon.py
	python3 assets/generate_icon.py

build:
	mkdir -p build dist

build/adhan_tests: build tests/test_main.cpp $(CORE_SRCS)
	$(HOST_CXX) $(HOST_CXXFLAGS) -o $@ tests/test_main.cpp $(CORE_SRCS)

test: build/adhan_tests
	./build/adhan_tests

test-tz: build/adhan_tests
	TZ=UTC ./build/adhan_tests
	TZ=America/New_York ./build/adhan_tests
	TZ=Europe/London ./build/adhan_tests

build/app.o: icon src/win/app.rc src/win/app.manifest src/win/resource.h assets/app.ico | build
	$(MINGW_WINDRES) -I . -I src/win -O coff -o $@ src/win/app.rc

build/AdhanVolume.exe: $(CORE_SRCS) $(WIN_SRCS) build/app.o | build
	$(MINGW_CXX) $(MINGW_CXXFLAGS) -o $@ $(CORE_SRCS) $(WIN_SRCS) build/app.o \
		$(MINGW_LDFLAGS) $(MINGW_LIBS)
	$(MINGW_PREFIX)-strip $@

windows: build/AdhanVolume.exe
	mkdir -p dist
	cp -f build/AdhanVolume.exe dist/AdhanVolume.exe
	cp -f build/AdhanVolume.exe dist/AdhanVolume-x64.exe
	@echo "Built dist/AdhanVolume.exe (x64) and dist/AdhanVolume-x64.exe"

build/AdhanVolume-x86.exe: $(CORE_SRCS) $(WIN_SRCS) icon src/win/app.rc | build
	$(MINGW32_WINDRES) -I . -I src/win -O coff -o build/app32.o src/win/app.rc
	$(MINGW32_CXX) -std=c++11 -O2 -Wall -I src -I src/win -DUNICODE -D_UNICODE \
		-DWINVER=0x0601 -D_WIN32_WINNT=0x0601 -municode \
		-o $@ $(CORE_SRCS) $(WIN_SRCS) build/app32.o \
		-static -static-libgcc -static-libstdc++ -mwindows $(MINGW_LIBS)
	$(MINGW32_PREFIX)-strip $@

windows-x86: build/AdhanVolume-x86.exe
	mkdir -p dist
	cp -f build/AdhanVolume-x86.exe dist/AdhanVolume-x86.exe
	@echo "Built dist/AdhanVolume-x86.exe"

build/AdhanVolume-arm64.exe: $(CORE_SRCS) $(WIN_SRCS) icon src/win/app.rc | build
	$(ARM64_WINDRES) -I . -I src/win -O coff -o build/app-arm64.o src/win/app.rc
	$(ARM64_CXX) -std=c++11 -O2 -Wall -Wextra -I src -I src/win -DUNICODE -D_UNICODE \
		-DWINVER=0x0601 -D_WIN32_WINNT=0x0601 -municode \
		-o $@ $(CORE_SRCS) $(WIN_SRCS) build/app-arm64.o \
		-static -mwindows $(MINGW_LIBS)
	- $(ARM64_STRIP) $@

windows-arm64:
ifeq ($(wildcard $(ARM64_CXX)),)
	@echo "ARM64 toolchain not found at $(ARM64_CXX)"
	@echo "Skipping native ARM64. Windows 10 ARM can run dist/AdhanVolume-x86.exe (x86 emulation)."
	@echo "Install llvm-mingw and set LLVM_MINGW, then re-run make windows-arm64."
else
	$(MAKE) build/AdhanVolume-arm64.exe
	mkdir -p dist
	cp -f build/AdhanVolume-arm64.exe dist/AdhanVolume-arm64.exe
	@echo "Built dist/AdhanVolume-arm64.exe (native ARM64, Windows 10/11 ARM)"
endif

windows-all: windows windows-x86 windows-arm64

dist: windows-all

clean:
	rm -rf build dist
	rm -f assets/app.ico
