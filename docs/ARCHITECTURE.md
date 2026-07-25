# Maclock Architecture

Maclock is ESP32-S3 firmware that drives a 320x240 color display inside a
Maclock enclosure. The firmware either presents a Macintosh-inspired clock UI
or boots a Macintosh Plus emulator, depending on a saved boot preference or a
selection in Boot Options.

PlatformIO builds the project with the Arduino framework for `lolin_s3`.
TFT_eSPI owns the ILI9341 panel, LVGL implements the normal interface, LittleFS
stores runtime media, and a prepared Mini vMac source tree supplies the emulator
core.

## High-Level Boot Flow

`setup()` in `src/main.cpp` is the composition root.

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

## Flash Layout And LittleFS

`partitions.csv` defines:

| Partition | Size | Purpose |
| --- | ---: | --- |
| `nvs` | 20 KiB | Arduino Preferences and platform state |
| `otadata` | 8 KiB | OTA metadata |
| `app0` | 3 MiB | Firmware image |
| `spiffs` | 12.94 MiB | LittleFS data partition |

Despite the partition subtype name, `board_build.filesystem = littlefs` makes
PlatformIO build and upload a LittleFS image from `data/`.

The filesystem contains two kinds of content:

- Tracked UI images, fonts, weather icons, plugin icons, and MP3 effects.
- Ignored local emulator inputs such as `vMac.ROM` and `disk1.dsk`.

Arduino callers use paths such as `/background.png`; LVGL reaches the same file
through the registered `S:` drive as `S:/background.png`. Mini vMac's file
adapter normalizes relative names to root-level LittleFS paths.

## Display Architecture

The physical TFT is 320x240 with rotation 3. Both modes reserve a 16-pixel top
border and a 16-pixel right border around a 304x224 logical surface.

### Clock mode

`setup_lvgl_display()` creates a 304x224 LVGL display with a full-frame RGB565
buffer used in partial render mode. `lvgl_to_TFT_eSPI()` flushes LVGL areas at a
physical Y offset of 16 pixels. It clears the panel once and then writes only
invalidated areas.

`lvgl_fs_init_littlefs()` registers drive `S:` using `fs::File` wrappers for
open, close, read, seek, and tell. `init_ui_assets()` decodes frequently reused
PNG assets once and retains duplicated LVGL draw buffers to reduce repeated
decode work.

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

`loop()` maintains an integer `UiState`, a state-entry timestamp, and small
state-specific counters. Event callbacks request transitions through
`g_requested_state`; the loop applies them before dispatching the next state.

| State | Behavior |
| --- | --- |
| `EMPTY_SCREEN` | Show the background/corners and start `startup.mp3`. |
| `WAIT_STARTUP_SOUND` | Wait for the audio task to report completion. |
| `WAIT_FLOPPY_1` / `WAIT_FLOPPY_2` | Alternate missing-disk artwork while waiting for the floppy input. |
| `FLOPPY_INSERTED` | Start `floppy.mp3` and lower codec volume. |
| `BOOT_PLUGINS` | Probe and progressively reveal codec, touch, weather, and RTC plugin icons. |
| `WAIT_FLOPPY_SOUND` | Wait for the floppy sound to finish. |
| `NORMAL` | Show the clock, date, weather, gauge, menus, and floppy indicator. |
| `SET_DATETIME` | Show the RTC date/time editor. |
| `BOOT_OPTIONS` | Select startup brightness, launch clock/emulator, optionally remember the default, or open diagnostics. |
| `EMULATOR` | Run Mini vMac synchronously and return to Boot Options after a safe exit. |
| `DIAGNOSTICS` | Live-test GPIO inputs, encoder, touch, charging, known I2C addresses, and RTC health. |
| `CALIBRATION` | Capture four raw FT6336 corner samples and persist their bounds. |

The plugin diagnostic is fail-stop by design. Each expected device must be
present. A missing device displays its icon in red and blinks forever instead
of advancing to the clock.

In `NORMAL`, the UI refreshes at most every 100 ms, while
`update_clock_labels()` suppresses work until the RTC second changes. Pressing
and releasing the clock button opens the date/time editor. Holding Clock and
Alarm together for two seconds opens Boot Options. The floppy level controls
the small disk icon.

## UI Composition

`init_ui_assets()` creates one LVGL screen containing:

- Background and corner-frame images.
- Startup missing-disk and boot/plugin layers.
- A white menu bar with left/right image fragments.
- Clock, time, and date labels using generated Chicago-style LVGL fonts.
- Temperature text, weather icon, and pressure/humidity gauge.
- Date/time editor, boot-options and diagnostics panels, calibration
  label/crosshair, and a touch cursor that hides after two seconds.

`hide_all_ui()` is the common transition primitive. It hides every layer before
the active state reveals its own set, preventing stale state-specific objects
from remaining visible.

`datetime_ui.cpp` owns its widgets and styles but deliberately leaves RTC
ownership in `main.cpp`. Saving calls `rtc_adjust_datetime()`; Save and Cancel
both request a transition back to `NORMAL`.

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

`input_task` runs every 20 ms on core 1:

- Floppy is retained as a level.
- Alarm, clock, and discrete touch are published as rising-edge events.
- A FreeRTOS critical section protects the shared `InputState`.

The loop consumes one-shot edges and clears them while retaining the floppy
level. The encoder count is clamped to 0–12 and mapped through a perceptual
backlight PWM curve. Brightness changes are written to Preferences after a
500 ms debounce. A touch edge overrides PWM to full brightness for ten seconds.

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
| `0x47` | BMP580/BMP581 | Temperature and pressure |
| `0x68` | DS1307 or DS3231 | Clock and calendar |

Weather detection prefers BMP5xx at `0x47`. If it is absent, HTU2x at `0x40`
is attempted. BMP mode maps pressure from 980–1040 hPa to the gauge and weather
icon; HTU mode maps 0–100% relative humidity.

RTC detection first checks for an ACK at `0x68`, then probes control-register
behavior to distinguish DS1307 from DS3231. `rtc_now()` returns a fixed
2000-01-01 value when no supported RTC is active. Boot Options, Diagnostics,
and serial output report stopped DS1307 clocks, DS3231 lost-power status,
invalid dates, and dates earlier than 2024.

## Audio

Normal mode initializes the ES8311 codec and an `AudioOutputI2S` instance at
44.1 kHz. The main loop creates `AudioFileSourceLittleFS` and
`AudioGeneratorMP3` objects for startup/floppy effects.

`audio_task` runs every 10 ms on core 0. It advances the decoder and publishes a
completion flag under `g_mp3_mux`, allowing the UI state machine to wait without
performing decoding itself.

Mini vMac produces unsigned 8-bit mono PCM at its native 22,255 Hz rate.
`src/minivmac_OSGLUE.c` forwards each completed 512-sample block to the Arduino
bridge, which converts it to signed 16-bit samples and writes it to both I2S
channels. The ES8311 uses the divider ratio from its 22,050 Hz table entry while
the I2S peripheral supplies the exact Mini vMac clock.

The bridge initializes the codec on demand for a saved-default emulator boot,
stops and reconfigures the shared `AudioOutputI2S` object for Mini vMac, and
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
| NVS Preferences, namespace `maclock` | Encoder brightness, boot-brightness choice, default emulator/clock choice |
| EEPROM emulation | FT6336 calibration structure |
| LittleFS | UI assets, audio, ROM, and mutable emulator disk images |

LittleFS contains user-significant emulator disks. Automatic formatting,
wholesale filesystem replacement, and filesystem upload are potentially
destructive even when the firmware image itself is safe to replace.

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

There is no host-side unit or integration test suite. Hardware validation should
cover the mode and subsystem changed:

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
