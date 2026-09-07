# Maclock

Maclock replaces the original Maclock screen with a 320×240 color IPS display
driven by an ESP32-S3.

It combines two experiences:

- **Clock mode** — a Macintosh-inspired clock with multiple faces, alarms,
  timers, weather, chimes, night mode, and persistent settings.
- **Mini vMac mode** — a Macintosh Plus emulator using ROM and disk images
  stored in LittleFS.

<p align="center">
  <img src="img/final_front.jpg" alt="Completed Maclock showing the clock interface" width="420">
</p>

## Documentation

- **User Manual:** [English](manual.md) · [Français](manual.fr.md) ·
  [Español](manual.es.md) · [Deutsch](manual.de.md) ·
  [Italiano](manual.it.md)
- **[Build Your Own](BUILD.md)** — required hardware, disassembly, wiring,
  firmware preparation, and flashing.
- **[Troubleshooting](TROUBLESHOOTING.md)** — startup, touchscreen, RTC,
  filesystem, emulator, and network problems.
- **[Architecture](docs/ARCHITECTURE.md)** — firmware and service design.

## Highlights

- Thirteen built-in clock-face projects, plus web Clock Face and Loading Screen
  Editors for project-local assets, localized text, and conditional layers.
- Light and dark themes, configurable accents and numeral sizes.
- Three weekly alarms, a countdown timer, hourly or quarter-hour chimes, and
  selectable MP3 sounds.
- Scheduled dimming or screen-off night mode.
- Local BMP5xx or HTU2x weather readings plus optional online forecasts.
- A responsive local web control panel, MQTT, and Home Assistant discovery.
- Verified firmware and filesystem updates over HTTPS.
- A Macintosh Plus emulator with persistent writable disk images.
- English, French, Spanish, German, and Italian interfaces.
- Appearance selects the loading-screen project. A project can provide its own
  boot sound and volume; invalid or missing selections fall back safely to the
  first valid loading project, then to the legacy boot screen.

## Discord

Join the community on the [Discord server](https://discord.gg/89etSPMFym).

## Desktop Simulator

The `maclock-local` CMake target runs the complete application on macOS and
Windows 10/11 x64:
the real LVGL configuration and clock screens, both web portals, audio, and
Mini vMac all use the same application code as the ESP32 firmware. SDL3 stores
the complete 320×240 RGB565 framebuffer but presents only Maclock's active
304×224 viewport. A Dear ImGui side panel simulates the attached hardware.

The first configure downloads pinned host-only dependencies, so it requires an
Internet connection. Xcode Command Line Tools and CMake 3.25 or newer are
required. Windows builds use the MSYS2 UCRT64 environment; install the
prerequisites from an MSYS2 shell with:

```sh
pacman -S --needed git make patch tar wget \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-zlib
```

Then open the **MSYS2 UCRT64** shell, not the plain MSYS shell:

```sh
./prepare.sh
cmake --preset windows-ucrt64-release
cmake --build --preset windows-ucrt64-release
"./build/windows-ucrt64-release/Maclock Simulator.exe"
ctest --preset windows-ucrt64-release
```

The Windows simulator is a console application so diagnostics remain visible.
Use `windows-ucrt64-debug` only for debugging; its unoptimized LVGL and Mini
vMac code is substantially slower and can cause audio underruns.

```sh
cmake --preset macos-debug
cmake --build --preset macos-debug
open "build/macos-debug/Maclock Simulator.app"
```

Release and AddressSanitizer builds use `macos-release` and `macos-asan`.

```text
--startup config|clock|emulator|firmware
--data-dir PATH
--state-dir PATH
--reset-state
--http-port PORT
--scale auto|1|2|3|4
--floppy-inserted
```

The default startup is Configuration. `firmware` honors the saved boot
preference; the other startup values override it for that run. The display
uses the largest integer scale that fits the desktop unless `--scale`
overrides it.

Click or drag on the framebuffer to use the touchscreen path. Right-clicking
holds the discrete-touch input. The hardware panel provides the floppy level,
Alarm, Clock, Alarm + Clock, discrete touch, encoder steps, weather-sensor and
RTC controls. A mouse wheel over the display also turns the encoder.

Persistent desktop data lives at:

```text
~/Library/Application Support/Maclock Simulator
```

On Windows it lives at:

```text
%LOCALAPPDATA%\Fensoft\Maclock Simulator
```

Preferences use an atomically replaced typed file, EEPROM uses a binary image,
and LittleFS overlays writes on the repository's read-only `data/` directory.
The control panel is available on `http://127.0.0.1:8088/` by default. The
overlay keeps deletion markers for source files; recreating a directory clears
its stale markers so source loading and clock-face projects are enumerated
again. `--reset-state` removes only the resolved simulator state
directory.

The simulated Wi-Fi network is `Mac Host Network`; connection succeeds with
any credentials. A fresh state starts connected with Paris selected for
online weather. Firmware port 80 maps to `http://127.0.0.1:8088/` by default.

Run the desktop smoke tests with:

```sh
ctest --test-dir build/macos-debug --output-on-failure
```

## Web Control Panel Development

The responsive Vue source is in `web/control-panel/`. For web-only
development:

```sh
cd web/control-panel
npm install
npm run dev
npm run i18n:check
npm run build
```

The development server supplies sample device state. PlatformIO runs
`scripts/build_control_panel.py` automatically when the embedded web header is
stale. `npm run i18n:check` verifies shared web translation coverage; `npm run
build` runs that check, builds the Vue bundle, and regenerates
`src/control_panel_page.h`. Do not edit the generated header by hand.

## Firmware Development

Prepare generated Mini vMac sources and local assets once:

```sh
./prepare.sh
```

Audit project assets, then build the firmware and LittleFS image:

```sh
node scripts/audit_littlefs_assets.mjs
pio run -e lolin_s3
pio run -e lolin_s3 -t buildfs
```

The audit checks referenced static, weather, and I2C-module project assets but
does not delete files. `data/` is the source LittleFS image; the desktop
simulator writes instead to its state overlay.

Do not upload without confirming the intended serial device. ROMs and disk
images may contain licensed or user-modified data and must not be published
accidentally.

See [AGENTS.md](AGENTS.md) for repository-specific development guidance.
