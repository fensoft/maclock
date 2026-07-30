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

- **[User Manual](manual.md)** — controls, daily operation, configuration,
  alarms, connectivity, updates, and Mini vMac.
- **[Build Your Own](BUILD.md)** — required hardware, disassembly, wiring,
  firmware preparation, and flashing.
- **[Troubleshooting](TROUBLESHOOTING.md)** — startup, touchscreen, RTC,
  filesystem, emulator, and network problems.
- **[Architecture](docs/ARCHITECTURE.md)** — firmware and service design.

## Highlights

- Five clock faces, light and dark themes, configurable accents and numeral
  sizes.
- Three weekly alarms, a countdown timer, hourly or quarter-hour chimes, and
  selectable MP3 sounds.
- Scheduled dimming or screen-off night mode.
- Local BMP5xx or HTU2x weather readings plus optional online forecasts.
- A responsive local web control panel, MQTT, and Home Assistant discovery.
- Verified firmware and filesystem updates over HTTPS.
- A Macintosh Plus emulator with persistent writable disk images.
- English, French, Spanish, German, and Italian interfaces.

## Discord

Join the community on the [Discord server](https://discord.gg/89etSPMFym).

## macOS Desktop Simulator

The `maclock-local` CMake target runs the complete application on macOS:
the real LVGL configuration and clock screens, both web portals, audio, and
Mini vMac all use the same application code as the ESP32 firmware. SDL3 stores
the complete 320×240 RGB565 framebuffer but presents only Maclock's active
304×224 viewport. A Dear ImGui side panel simulates the attached hardware.

The first configure downloads pinned host-only dependencies, so it requires an
Internet connection. Xcode Command Line Tools and CMake 3.24 or newer are
required.

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

Preferences use an atomically replaced typed file, EEPROM uses a binary image,
and LittleFS overlays writes on the repository's read-only `data/` directory.
`--reset-state` removes only the resolved simulator state directory.

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
```

The development server supplies sample device state. PlatformIO runs
`scripts/build_control_panel.py` automatically when the embedded web header is
stale; do not edit `src/control_panel_page.h` by hand.

## Firmware Development

Prepare generated Mini vMac sources and local assets once:

```sh
./prepare.sh
```

Build the firmware and LittleFS image:

```sh
pio run -e lolin_s3
pio run -e lolin_s3 -t buildfs
```

Do not upload without confirming the intended serial device. ROMs and disk
images may contain licensed or user-modified data and must not be published
accidentally.

See [AGENTS.md](AGENTS.md) for repository-specific development guidance.
