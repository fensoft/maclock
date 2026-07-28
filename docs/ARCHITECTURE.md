# Maclock Architecture

Maclock is ESP32-S3 firmware that drives a 320x240 color display inside a
Maclock enclosure. The firmware either presents a Macintosh-inspired clock UI
or boots a Macintosh Plus emulator, depending on a saved boot preference or a
selection in Boot Options.

PlatformIO builds the project with the Arduino framework for `lolin_s3`.
TFT_eSPI owns the ILI9341 panel, LVGL implements the normal interface, LittleFS
stores runtime media, and a prepared Mini vMac source tree supplies the emulator
core.

## Platform HAL

`MaclockApp` receives a `MaclockHal&`; `begin()` and `tick()` remain its only
application entry points. The HAL is divided into narrow runtime/timing, GPIO,
I²C, SPI, display, audio, storage, and networking interfaces.

- `Esp32MaclockHal` is the firmware composition adapter. Arduino, Espressif,
  TFT_eSPI, codec, FreeRTOS, and filesystem behavior remains owned by the
  existing physical services, so pin definitions, task parameters,
  Preferences keys, and timing are unchanged.
- `LocalMaclockHal` installs desktop compatibility implementations before
  `MaclockApp::begin()`. Existing `TwoWire`, `SPIClass`, `TFT_eSPI`,
  `Preferences`, `EEPROM`, FreeRTOS, Espressif I²S/heap/timer, Wi-Fi, HTTP, and
  Arduino GPIO calls then delegate to the local HAL. Mini vMac's generated core
  remains unchanged and its existing Arduino bridge compiles against those
  shims.

The desktop target is a separate CMake-built macOS application. PlatformIO's
`lolin_s3` environment does not include any SDL, ImGui, miniaudio, host
filesystem, or host networking source.

### LocalMaclockHal runtime

SDL3 is initialized before application setup. SDL events and Dear ImGui are
pumped from the normal loop and from Arduino `yield()`/`delay()`, keeping the
window responsive during startup audio and synchronous Mini vMac execution.
The synchronized framebuffer remains 320×240 RGB565, while the SDL image
samples the active 304×224 region at `(0, 16)` to hide unused panel edges.
Touch coordinates are translated back into full-frame coordinates. Unless
explicitly overridden, the window selects the largest integer framebuffer
scale that fits the primary display's usable bounds. Presentation preserves
logical RGB565 words and applies the current backlight PWM to each channel
without modifying the underlying framebuffer.

The local device model provides:

- active-low GPIO for Floppy, Alarm, Clock, and discrete touch plus an encoder;
- virtual SPI transactions and the TFT_eSPI operations used by LVGL and
  Mini vMac;
- FT6336 registers at `0x38`, populated by inverse-rotation mapping of SDL
  mouse press, drag, and release events;
- ES8311 registers at `0x18`, BMP5xx at selectable `0x47`/`0x50`, optional
  HTU2x at `0x40`, and selectable DS3231/DS1307 behavior at `0x68`;
- host-time RTC progression with a session-local adjustment offset;
- FreeRTOS task, mutex, event-group, stream-buffer, delay, and cooperative
  shutdown compatibility backed by C++ threads and condition variables;
- miniaudio output for decoded MP3 and Mini vMac PCM, with codec mute and a
  0–100% unity-or-lower gain path. Its bounded queue applies producer
  backpressure and drains through the final device callback before EOF can
  mute the codec;
- a deliberately stable unavailable/not-charging battery result.

The default `config` launch holds Clock only through boot sampling, then
releases it before the input task starts. `emulator` and `clock` override the
saved startup choice for the current process; `firmware` uses the saved
preference. Desktop Alarm and Clock clicks remain active-low for at least
120 ms so the normal 20 ms input task cannot miss them; discrete touch remains
active longer to pass its production filtering. Window close requests assert
Clock and Alarm together long enough to use the existing Mini vMac safe-exit
path.

### Desktop persistence and networking

The local Preferences file is typed and atomically replaced. EEPROM uses a
persistent binary image. The LittleFS compatibility layer merges repository
`data/` as a read-only base with a writable state overlay and copies a base
file into the overlay before its first mutation. Directory enumeration merges
both layers. This prevents emulator disk writes or application changes from
modifying repository assets.

Simulated Wi-Fi scanning returns `Mac Host Network`; association is
credential-independent and IP/RSSI data is deterministic. When the local
Preferences namespace contains no Wi-Fi keys, `WifiService` seeds a
desktop-only enabled configuration for that network and Paris weather
coordinates. Existing keys always win, including a saved disabled state, and
resetting simulator state recreates the defaults. Firmware HTTP listeners are
remapped from port 80 to localhost port 8088 by default. Socket threads queue
requests so route callbacks execute from the application loop and therefore
retain the firmware's LVGL ownership rule. Outbound weather HTTP uses the host
network, while NTP reads host time. Captive DNS and mDNS are successful no-ops
rather than host network reconfiguration.

## High-Level Boot Flow

`src/main.cpp` is intentionally only the Arduino adapter: its static
`MaclockApp` forwards `setup()` to `MaclockApp::begin()` and `loop()` to
`MaclockApp::tick()`. `MaclockApp` is the composition root and owns settings,
I2C, RTC, weather, input, display, audio, Wi-Fi, alarm, timer, and date/time
services.

1. Start serial output at 115200 baud and keep the TFT backlight off.
2. Open the `maclock` Preferences namespace and load the boot-brightness and
   default-mode choices.
3. Enable external-memory allocation, mount LittleFS, initialize the TFT, and
   initialize both touch mechanisms.
4. Rotate the TFT, configure the FT6336 mapping and encoder, and apply the
   selected perceptual brightness level.
5. Sample the clock button to detect a boot-options request.
6. If boot options were not requested and the saved default selects Mini vMac,
   enter `minivmac()` regardless of the physical floppy-switch position.
7. After clock selection or an emulator return, initialize the codec, LVGL
   display/input/filesystem bridge, UI assets, I2C peripherals, FreeRTOS
   input/audio tasks, and weather sensor.
8. If the clock button was held or Mini vMac returned, request Boot Options;
   otherwise the normal clock startup state machine begins.

Mini vMac can also be launched synchronously from the `EMULATOR` loop state.
Its render resources are recreated for each launch and destroyed on return, so
the user can switch between Boot Options and the emulator repeatedly.

## Source And Build Composition

`platformio.ini` defines the single `lolin_s3` environment and pins the major
dependencies:

- TFT_eSPI 2.5.43.
- LVGL 9.4.
- ESP8266Audio for MP3 playback.
- ESP32Encoder.
- Adafruit RTClib, BMP5xx, and HTU21DF libraries.

The build flags also configure the panel, I2S/I2C buses, GPIO inputs, touch
controller, logical display dimensions, and Mini vMac includes. The emulator is
built with aggressive speed optimization, no RTTI, and no exceptions.

`prepare.sh` creates the ignored `src/minivmac/` tree from the Mini vMac 36.04
archive and applies patches under `patches/`. PlatformIO compiles that upstream
tree while filtering out desktop OS glue and emulated devices that do not apply
to the selected Macintosh Plus configuration.

The project-specific emulator boundary remains tracked:

- `src/minivmac_OSGLUE.c` adapts Mini vMac's host operations.
- `src/minivmac_ArduinoAPI.cpp` implements those operations with Arduino,
  FreeRTOS, TFT_eSPI, LittleFS, and PSRAM.
- `include/minivmac/*.h` records the configured emulator model and build
  contract.
- `patches/` makes changes to generated upstream sources reproducible.

Application code is divided by ownership:

- `SettingsStore` owns the `maclock` Preferences namespace and validation.
- `I2cBus`, `RtcService`, `WeatherService`, `InputService`, `DisplayService`,
  and `AudioService` own hardware-facing state.
- `WifiService`, `AlarmService`, and `TimerService` own their worker and
  persisted runtime state.
- `ControlPanelService` owns a station-only HTTP server and mDNS advertisement.
  It is never used as the captive Wi-Fi setup server.
- `UiShell`, `StartupView`, `ClockView`, `BootOptionsView`,
  `DateTimeEditor`, `AlarmView`, `TimerView`, `DiagnosticsView`,
  `WifiSetupView`, and `CalibrationView` own UI state.
- `SoundSelector` is a reusable stateful widget shared by alarm and chime
  configuration.

The focused files under `src/ui/` are guarded implementation units included by
`maclock_app.cpp`. This keeps each source unit small while preserving a single
translation unit for private LVGL callbacks and their instance compatibility
thunks. PlatformIO may discover those `.cpp` files separately; without
`MACLOCK_COMBINED_SOURCE` they intentionally compile empty.

## Web Portals

Maclock has two intentionally independent HTTP lifecycles:

- `WifiService` starts the `Maclock Setup` access point, DNS responder, and
  captive setup server only while `UiState::WifiSetup` is active. Its form
  changes Wi-Fi credentials and the forecast city.
- `ControlPanelService` starts only after station Wi-Fi has connected. It
  advertises `maclock.local`, serves the embedded responsive control page, and
  exposes JSON/form routes for appearance, alarms, timer, night mode, chimes,
  and sound previews.

`MaclockApp` implements `ControlPanelEventSink`. HTTP callbacks run from
`MaclockApp::tick()`, validate ranges and LittleFS sound paths, then call the
same state-owning services and persistence methods used by the device UI.
They never access LVGL from the Wi-Fi worker. The control server is stopped
before the setup portal starts or Mini vMac runs, avoiding two listeners on
port 80 and preserving exclusive display/audio ownership.

## Flash Layout And LittleFS

`partitions.csv` defines:

| Partition | Size | Purpose |
| --- | ---: | --- |
| `nvs` | 20 KiB | Arduino Preferences and platform state |
| `otadata` | 8 KiB | OTA metadata |
| `app0` | 3 MiB | Running or inactive firmware image |
| `app1` | 3 MiB | Running or inactive firmware image |
| `spiffs` | 9.94 MiB | LittleFS data partition |

Despite the partition subtype name, `board_build.filesystem = littlefs` makes
PlatformIO build and upload a LittleFS image from `data/`.

The filesystem contains three kinds of content:

- Tracked UI images, fonts, weather icons, plugin icons, and MP3 effects.
- Ignored local emulator inputs such as `vMac.ROM` and `disk1.dsk`.
- User MP3s in `/downloaded/`, the only subtree protected from release
  reconciliation.

Arduino callers use paths such as `/background.png`; LVGL reaches the same file
through the registered `S:` drive as `S:/background.png`. Mini vMac's file
adapter normalizes relative names to root-level LittleFS paths.

## Update Architecture

`UpdateService` owns release discovery, semantic-version comparison, ETag
caching, update progress, error state, raw firmware uploads, and rollback
validation. It checks GitHub only after station Wi-Fi connects. Its immutable
snapshot is consumed by both `MaclockApp` and `ControlPanelService`; neither
the update worker nor HTTP callbacks touch LVGL.

Release assets are a deterministic ZIP32 archive generated only from tracked
`data/` files. `scripts/package_release.py` rejects tracked
`data/downloaded` files, symlinks, traversal paths, ZIP64, encryption, and data
descriptors. Its manifest records the path, uncompressed and compressed size,
method, and SHA-256 of every release-owned file.

The firmware reader accepts stored and raw-DEFLATE entries and operates
forward-only. A 32 KiB dictionary is the largest decompression allocation.
Unchanged files are consumed without inflation. Changed files are written to a
single temporary file, verified, and atomically renamed. Only after the whole
archive verifies are obsolete release files deleted. `/downloaded/` is
excluded from manifest validation, replacement, deletion, and free-space
reclamation. An NVS work marker causes an interrupted reconciliation to restart
the stream and skip files that already match.

Firmware is accepted only when its ESP image metadata identifies an ESP32-S3
Maclock application and it fits the inactive 3 MiB OTA partition. Official
downloads additionally require manifest size and SHA-256 matches over
validated HTTPS. The rollback-enabled bootloader leaves a new image pending
until LittleFS, settings, LVGL initialization, and ten seconds of the main loop
have succeeded; only then does the application mark it valid.

The web panel exposes `/api/update/status`, `/api/update/check`,
`/api/update/install`, `/api/update/firmware`, and `/api/update/dismiss`.
These routes do not exist in the separate captive Wi-Fi setup portal. The
desktop simulator performs discovery and comparison but reports firmware
installation as unavailable.

## Display Architecture

The physical TFT is 320x240 with rotation 3. Both modes reserve a 16-pixel top
border and a 16-pixel right border around a 304x224 logical surface.

### Clock mode

`DisplayService::beginLvgl()` creates a 304x224 LVGL display with a full-frame
RGB565 buffer used in partial render mode. Its flush thunk recovers the
`DisplayService` instance from LVGL user data and writes areas at a physical Y
offset of 16 pixels. It clears the panel once and then writes only invalidated
areas.

`DisplayService::registerLittleFs()` registers drive `S:` using `fs::File`
wrappers for open, close, read, seek, and tell. `UiShell::init()` decodes
frequently reused PNG assets once and retains duplicated LVGL draw buffers to
reduce repeated decode work.

All LVGL object creation and mutation occurs in the Arduino setup/loop context.
The FreeRTOS input task publishes simple input state rather than touching LVGL.

### Emulator mode

Mini vMac is configured for a 304x224 one-bit screen. When its core reports a
changed screen, `ArduinoAPI_DrawScreen()` publishes the screen pointer and wakes
`RenderTask`.

`RenderTask` converts the monochrome bitmap to RGB565 line by line:

- Set bits become black; clear bits become white.
- The emulated image begins below a 16-pixel black top border.
- The remaining 16-pixel strip on the right is black.
- A single 320-pixel line buffer avoids allocating a full RGB565 frame.

The task writes the full physical 320x240 display through TFT_eSPI while holding
the emulator's render and SPI locks. For the first four seconds it draws a
control overlay above the emulated screen.

## Normal Clock State Machine

`MaclockApp::tick()` maintains a typed `UiState`, state-entry timestamp, and
common scheduling state as class members. Views report transitions and RTC,
alarm, or timer actions through `AppEventSink`, implemented by `MaclockApp`.
LVGL callbacks are static thunks with instance context at the boundary.

| State | Behavior |
| --- | --- |
| `EMPTY_SCREEN` | Show the background/corners and start `startup.mp3`. |
| `WAIT_STARTUP_SOUND` | Wait for the audio task to report completion. |
| `WAIT_FLOPPY_1` / `WAIT_FLOPPY_2` | Alternate missing-disk artwork while waiting for the floppy input. |
| `FLOPPY_INSERTED` | Start `floppy.mp3` and lower codec volume. |
| `BOOT_PLUGINS` | Probe and progressively reveal codec, touch, weather, and RTC plugin icons. |
| `WAIT_FLOPPY_SOUND` | Wait for the floppy sound to finish. |
| `NORMAL` | Show the clock, date, weather, gauge, menus, and floppy indicator. |
| `ALARM_EDITOR` / `ALARM_RINGING` | Configure alarms or run snooze/dismiss playback. |
| `TIMER_EDITOR` / `TIMER_FINISHED` | Configure the timer or run completion playback. |
| `BOOT_OPTIONS` | Show the four-section Configuration hub and its section-local General, Display, Sound, and System pages. |
| `EMULATOR` | Run Mini vMac synchronously and return to Boot Options after a safe exit. |
| `DIAGNOSTICS` | Live-test GPIO inputs, encoder, touch, charging, known I2C addresses, and RTC health. |
| `WIFI_SETUP` | Show a standard open-network Wi-Fi QR and run the optional iOS/Android-compatible captive portal. |
| `CALIBRATION` | Capture four raw FT6336 corner samples and persist their bounds. |

The plugin diagnostic is fail-stop by design. Each expected device must be
present. A missing device displays its icon in red and blinks forever instead
of advancing to the clock.

In `NORMAL`, the UI refreshes at most every 100 ms, while `ClockView`
suppresses label work until the snapshot RTC second changes. `MaclockApp`
builds immutable clock and diagnostics snapshots, so those views do not probe
RTC, weather, input, Wi-Fi, or I2C hardware directly. Pressing and releasing
the clock button opens Configuration on a four-section hub. General has three
pages, Display five, Sound four, and System five; navigation and page counts
are local to the selected section. Its Date / Time page shows live RTC fields
and writes each **-**/**+** adjustment immediately.
The Regional page persists date order, 12/24-hour, and temperature-unit
choices. The adjacent Display page persists leading-zero, optional localized
three-letter weekdays, seconds, and light/dark theme. The shared
date formatter supplies Macintosh, Compact, Analog, and Flip, switching those
date labels from the 32-pixel font to a 24-pixel 1-bpp Chicago font while the
weekday is visible. `ClockView` applies 12/24-hour formatting to Macintosh,
Compact Digital, and Flip; seconds apply to all three digital faces, while
leading zero remains Compact/Flip-specific, with up to six independently
animated Flip cards.
Holding Clock and Alarm together for two seconds remains an alternate
Configuration shortcut. The floppy level controls the small disk icon.

## UI Composition

`UiShell::init()` creates one LVGL screen containing:

- Background and corner-frame images.
- Startup missing-disk and boot/plugin layers.
- A white menu bar with left/right image fragments.
- Clock, time, and date labels using generated Chicago-style LVGL fonts.
- Temperature text, weather icon, and pressure/humidity gauge.
- Date/time editor, boot-options and diagnostics panels, calibration
  label/crosshair, and a touch cursor that hides after two seconds.

`UiShell::hideAll()` is the common transition primitive. It hides every layer
before the active state reveals its own set, preventing stale state-specific
objects from remaining visible.

`DateTimeEditor` owns its widgets and styles but deliberately leaves RTC
ownership in `RtcService`. Save sends the new value through `AppEventSink`;
Save and Cancel request a transition back to `NORMAL`.

## Input Architecture

There are two touch inputs with distinct roles.

### FT6336 I2C controller

`src/touch.cpp` wraps the FT6336 driver and supplies:

- Mapped absolute coordinates for LVGL.
- Raw coordinates for calibration and emulator relative motion.
- EEPROM-backed minimum/maximum calibration bounds.

The four-corner calibration records one sample at each corner after a
press/release cycle. It derives min/max X and Y bounds, updates the mapper, and
writes a structure identified by the `TOUC` magic value.

### Discrete touch input

`TouchSensor` on GPIO 2 is polled by `input_task`. A rising edge temporarily
forces full display brightness in clock mode. In emulator mode it is the
Macintosh mouse button.

The emulator obtains relative pointer movement by subtracting consecutive raw
FT6336 samples in `MouseClass::Read()`. Releasing the panel resets the prior
sample so the next touch starts with zero delta.

### Buttons and encoder

`InputService` runs its task every 20 ms on core 1:

- Floppy is retained as a level.
- Alarm, clock, and discrete touch are published as rising-edge events.
- A FreeRTOS critical section protects the shared `InputState`.

`MaclockApp::tick()` consumes one-shot edges and clears them while retaining
the floppy level. The encoder count is clamped to 0–12 and mapped through a
perceptual backlight PWM curve. Brightness changes are written through
`SettingsStore` after a 500 ms debounce. A touch edge overrides PWM to full
brightness for ten seconds.

The alarm edge is collected but currently has no state-machine behavior.
The normal state reads both button levels directly for the two-second
Clock+Alarm shortcut, delaying the Clock-only action until release so the chord
can be distinguished.

## I2C Devices And Sensor Selection

The shared bus uses SDA 16 and SCL 15. The codec setup initializes it at
100 kHz before the later device probes.

| Address | Device | Role |
| --- | --- | --- |
| `0x18` | ES8311 | Normal-mode MP3 output |
| `0x38` | FT6336 | Touch coordinates |
| `0x40` | HTU2x | Temperature and humidity |
| `0x47` or `0x50` | BMP580/BMP581 | Temperature and pressure |
| `0x68` | DS1307 or DS3231 | Clock and calendar |

Weather detection prefers BMP5xx at `0x47`, then `0x50`. If neither is
available, HTU2x at `0x40` is attempted. BMP mode maps pressure from 980–1040
hPa to the gauge and weather icon; HTU mode maps 0–100% relative humidity.

RTC detection first checks for an ACK at `0x68`, then probes control-register
behavior to distinguish DS1307 from DS3231. `rtc_now()` returns a fixed
2000-01-01 value when no supported RTC is active. Boot Options, Diagnostics,
and serial output report stopped DS1307 clocks, DS3231 lost-power status,
invalid dates, and dates earlier than 2024.

## Audio

`DisplayService` owns the ES8311 codec and `AudioOutputI2S` instance, initialized
at 44.1 kHz for normal mode. `AudioService` owns
`AudioFileSourceLittleFS`/`AudioGeneratorMP3` playback for clock sounds.

The `AudioService` task runs on core 0. It advances the decoder and publishes a
completion flag under its instance lock, allowing the UI state machine to wait
without performing decoding itself.

Mini vMac produces unsigned 8-bit mono PCM at its native 22,255 Hz rate.
`src/minivmac_OSGLUE.c` forwards each completed 512-sample block to the Arduino
bridge, which converts it to signed 16-bit samples and writes it to both I2S
channels. The ES8311 uses the divider ratio from its 22,050 Hz table entry while
the I2S peripheral supplies the exact Mini vMac clock.

`EmulatorHardwareBridge` exposes only the shared TFT, codec, and audio accessors
needed by Mini vMac. It initializes the codec on demand for a saved-default
emulator boot, stops and reconfigures the shared `AudioOutputI2S` object, and
restores its 44.1 kHz clock-mode configuration on exit. When Mini vMac is
launched from Boot Options, the normal `audio_task` remains alive but its MP3
decoder is stopped, leaving I2S ownership with the synchronous emulator path.

## Mini vMac Runtime

`minivmac()` creates the emulator event group and mutexes, starts `RenderTask`,
initializes the touch-backed mouse, and calls `minivmac_main()`. Holding Clock
and Alarm together for two seconds ejects and closes every mounted disk, sets
Mini vMac's shutdown flag, and returns to Boot Options. Shutdown resets the
render task and synchronization objects, while the next launch resets Mini
vMac's shutdown flag.

The selected configuration emulates a Macintosh Plus with:

- 68000-class CPU behavior (`Use68020 0`).
- A 128 KiB ROM named `vMac.ROM`.
- A 304x224 monochrome display.
- Up to six disk-image slots.
- Unsigned 8-bit mono sound at 22,255 Hz.
- No FPU, MMU, ADB, or secondary VIA.

At startup, `LoadInitialImages()` opens sequential `diskN.dsk` files starting
at 1 and stops at the first missing number. Images are opened read/write when
possible and read-only otherwise.

The Arduino API supplies:

- Millisecond time and cooperative yield/delay.
- LittleFS-backed C-style file operations.
- Default-heap or PSRAM allocation; allocations at least 256 KiB use PSRAM.
- Touch mouse motion/button state.
- Screen invalidation and render-task wakeup.

Mini vMac's main loop runs emulation ticks near 60.14742 Hz, services touch
input, and requests rendering when the monochrome screen changes.

## Concurrency And Ownership

| Context | Core | Responsibility |
| --- | ---: | --- |
| Arduino setup/loop | Arduino default | Boot decision, normal state machine, LVGL, RTC/weather reads, brightness, synchronous emulator PCM output |
| `input_task` | 1 | GPIO/discrete-touch polling and edge publication |
| `audio_task` | 0 | Normal-mode MP3 decoder advancement |
| `RenderTask` | 0 | Emulator bitmap conversion and TFT writes |
| Mini vMac main loop | Calling Arduino task | CPU/device emulation and event checks |

Synchronization boundaries:

- `g_input_state_mux` protects cross-context input state.
- `g_mp3_mux` protects the audio completion flag.
- `RenderTaskLock` prevents screen-buffer/render handoff races.
- `SPIBusLock` serializes emulator LittleFS operations and TFT rendering.

Only `minivmac()` creates `RenderTask`. A saved-default emulator launch happens
before the clock-mode input/audio tasks exist; an emulator launch from Boot
Options happens while those tasks are idle in the background. The Arduino
loop is blocked inside Mini vMac, so LVGL is not mutated concurrently with the
emulator renderer.

## Persistent State

Three storage mechanisms have different lifetimes:

| Store | Data |
| --- | --- |
| NVS Preferences, namespace `maclock` | Appearance, regional/time-format options, alarms, timer defaults, night/chime schedules, sound paths/volumes, Wi-Fi, brightness, and boot choice |
| EEPROM emulation | FT6336 calibration structure |
| LittleFS | UI assets, audio, ROM, and mutable emulator disk images |

LittleFS contains user-significant emulator disks. The one-time 1.0.0 USB
repartition and filesystem upload are deliberately destructive. Normal OTA
reconciliation overwrites release-owned content and removes other root files,
but never changes `/downloaded/`.

## Development And Verification

Prepare generated inputs once in a fresh checkout:

```bash
./prepare.sh
```

Build firmware and, when relevant, the filesystem:

```bash
pio run -e lolin_s3
pio run -e lolin_s3 -t buildfs
```

PlatformIO may be available as `~/.platformio/penv/bin/pio` when it is not on
the shell `PATH`.

Run deterministic release-package tests with:

```bash
python3 -m unittest tests/test_release_package.py
```

Hardware validation should cover the mode and subsystem changed:

- Cold boot into clock mode and emulator mode.
- Boot-options entry by holding the clock button.
- Start either mode with both floppy-switch positions and verify remembered
  defaults.
- Safely exit and relaunch Mini vMac repeatedly.
- Exercise every live hardware-diagnostics input and RTC warning.
- Startup plugin discovery and missing-device diagnostic.
- FT6336 pointer mapping, discrete touch, and four-point calibration.
- Encoder brightness, touch wake, and persistence across restart.
- RTC read/write and the applicable weather sensor.
- MP3 playback through ES8311.
- Mini vMac ROM/disk mounting, pointer input, and screen refresh.

Uploading firmware or LittleFS requires an explicitly selected physical device.
Do not upload as an implicit verification step.

## Architectural Constraints To Preserve

- Keep clock and emulator modes mutually exclusive until shared-resource
  ownership is redesigned.
- Keep the 304x224 logical surface aligned identically in LVGL and Mini vMac.
- Keep Mini vMac upstream generation reproducible through `prepare.sh` and
  `patches/`.
- Keep ROM/disk images out of version control and protect mutable LittleFS data.
- Keep LVGL single-threaded and preserve the existing cross-core locks.
- Keep pin assignments centralized in `platformio.ini`.
