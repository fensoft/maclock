# Maclock

Maclock replaces the original Maclock screen with a 320x240 color IPS display
driven by an ESP32-S3.

The firmware has two operating modes:

- **Clock mode** presents a Macintosh-inspired clock with startup sounds,
  date/time editing, weather data, touch calibration, and adjustable
  brightness.
- **Mini vMac mode** emulates a Macintosh Plus from ROM and disk images stored
  in LittleFS.

<p align="center">
  <img src="img/final_front.jpg" alt="Completed Maclock showing the clock interface" width="420">
</p>

## Discord

Join the community on the [Discord server](https://discord.gg/89etSPMFym).

## Build Your Own

See the illustrated [BUILD.md](BUILD.md) guide for required hardware,
disassembly, wiring, firmware preparation, and flashing.

## macOS Desktop Simulator

The `maclock-local` CMake target runs the complete application on macOS:
the real LVGL configuration and clock screens, both web portals, audio, and
Mini vMac all use the same application code as the ESP32 firmware. SDL3
stores the complete 320×240 RGB565 framebuffer but presents only Maclock's
active 304×224 viewport, hiding the unused black top/right area. A Dear ImGui
side panel simulates the attached hardware.

The first configure downloads pinned host-only dependencies, so it requires an
Internet connection. Xcode Command Line Tools and CMake 3.24 or newer are
required.

```sh
cmake --preset macos-debug
cmake --build --preset macos-debug
open "build/macos-debug/Maclock Simulator.app"
```

Release and AddressSanitizer builds use `macos-release` and `macos-asan`
instead. The simulator accepts:

```text
--startup config|clock|emulator|firmware
--data-dir PATH
--state-dir PATH
--reset-state
--http-port PORT
--scale auto|1|2|3|4
--floppy-inserted
```

The default startup is Configuration. The display scale is chosen
automatically as the largest integer scale that fits the usable desktop;
`--scale` can override it. `firmware` preserves the saved device boot
preference; the other startup values override it for that run.
`--floppy-inserted` starts the active-low floppy input asserted, which is
useful when testing the full clock startup without waiting at the disk prompt.

Click or drag directly on the framebuffer to use the existing FT6336 touch
path. Right-clicking the framebuffer holds the discrete-touch input until the
mouse button is released; desktop Mini vMac reads it directly so double-clicks
remain distinct. The fixed hardware toolbar provides the floppy level,
held Alarm, Clock, Alarm + Clock, and discrete-touch inputs, encoder steps, and
the current encoder value. Every momentary control follows mouse-down and
mouse-up directly instead of generating a fixed-duration pulse. The
displayed framebuffer follows the firmware's real backlight PWM, with its
current percentage shown above the screen. The hardware panel uses the bundled
Chicago font and classic Macintosh monochrome controls. A mouse wheel over the
screen also turns the encoder. Mini vMac runs at the original 1× Macintosh
speed in the desktop simulator; the optimized ESP32 speed remains unchanged.
The I²C panel can
switch between BMP5xx at `0x47`/`0x50`, HTU2x at `0x40`, or a disconnected
sensor and can adjust temperature, pressure, and humidity. It can also switch
the RTC between DS3231 and DS1307, disconnect it, or reset its session-local
offset to host time.

By default, persistent data lives at:

```text
~/Library/Application Support/Maclock Simulator
```

Preferences use an atomically replaced typed file, EEPROM uses a binary image,
and LittleFS overlays writes on top of the repository's read-only `data/`
directory. Mini vMac disk writes therefore survive a restart without changing
repository ROMs, disks, MP3s, or images. `--reset-state` removes only the
resolved simulator state directory.

The simulated Wi-Fi network is `Mac Host Network`; connection succeeds with
any credentials and reports deterministic local values. A fresh simulator
state starts with Wi-Fi enabled on that network and Paris selected for online
weather, so neither portal is required before the local control panel works.
Changing or disabling Wi-Fi saves that choice; `--reset-state` restores the
defaults. These defaults are desktop-only and never change ESP32 credentials.
Outbound HTTP uses the host network, NTP uses host time, and firmware port 80
is mapped to `http://127.0.0.1:8088/` by default. Captive DNS and mDNS report
success but do not redirect host traffic. The desktop **Open Portal** control
opens the active localhost address. Battery state is intentionally unavailable
and has no simulator control.

Useful non-interactive smoke test:

```sh
ctest --test-dir build/macos-debug --output-on-failure
```

## Software Manual

### Controls

| Control | Clock mode | Mini vMac mode |
| --- | --- | --- |
| Rotary encoder | Adjust brightness from off to maximum | Adjust brightness from off to maximum |
| Clock button | Open Configuration; dismiss a ringing alarm; wake night mode | Enter key |
| Alarm button | Open alarms and timers; snooze a ringing alarm; wake night mode | Escape key |
| Clock + Alarm, held for 2 seconds | Open Configuration | Safely exit to Configuration |
| Floppy switch | Advance startup and show the floppy icon | No emulator action |
| Display touchscreen | Operate menus, show the pointer, and wake night mode | Move the Macintosh pointer using relative motion |
| Discrete touch sensor (red wire) | Force full brightness for 10 seconds; snooze a ringing alarm | Macintosh mouse button |

Brightness uses perceptual steps, so the lower levels change more gradually.
Brightness, night-mode settings, the optional default boot mode, and
touchscreen calibration are stored persistently.

### Choosing The Boot Mode

Maclock can start either the clock or Mini vMac, regardless of the physical
floppy-switch position. The saved default determines which mode starts at
power-on.

To open Configuration:

1. Press **Clock** from the normal clock screen; or
2. Turn Maclock off, hold **Clock**, turn Maclock on, and release the button
   when **Configuration** appears.

Configuration opens on a four-button section hub instead of one long page
sequence:

- **General**: Language, Regional, and Date / Time (3 pages).
- **Display**: Display, Clock Face, Face Style, Face Details, Screensaver, and
  the two Night pages (7 pages).
- **Sound**: Chime, Chime Sound, Chime Volume, and Quiet Hours (4 pages).
- **System**: Preferences, Start, Wi-Fi, Tools, Software Update, and About
  (6 pages).

Each section has fixed **Previous**, **Sections**, and **Next** footer slots;
the unavailable direction is left empty on the first or last page.
**Sections** returns to the hub from any settings page, and **Exit** on the hub
returns to the normal clock:

- **Brightness / Latest** restores the last encoder brightness.
- **Brightness / Lowest** starts at the lowest visible setting.
- **Brightness / Highest** starts at full brightness.
- **Clock** runs the normal Macintosh-style clock startup.
- **Emulator** launches Mini vMac immediately.
- **Remember** saves the selected start mode as the next power-on default;
  **One time** starts it without changing the default.
- **Language** switches the interface immediately between English, French,
  Spanish, Deutsch, and Italian. The selection is saved persistently.
- **Diagnostics** opens a live hardware test screen for both buttons, the
  floppy switch, encoder, touch sensor, charging input, known I²C devices, and
  RTC health.
- **Date / Time** shows live RTC fields; select a field and adjust it directly
  with the large **-** and **+** buttons.
- **Regional** selects date order, 12- or 24-hour display, and temperature
  unit in a five-button 3+2 layout.
- **Display** controls initial zero, the optional localized three-letter
  weekday, seconds, and light/dark clock theme with four large classic-Mac
  checkbox tiles in a 2×2 grid. Dates use a smaller crisp bitmap font while
  the weekday is visible. Hiding seconds also removes the Analog second hand.
- **Face Style** chooses a contrast-safe Default, red, orange, green, blue, or
  purple highlight and Small, Default, or Large clock numerals.
- **Face Details** controls weather visibility on the Macintosh and Compact
  faces plus Slow, Normal, or Fast Flip animation speed.
- **About** shows the fensoft logo, author, GitHub address, and a scannable
  project QR code.
- **Sections** returns to the configuration hub from any settings page.
- **Exit** returns from the hub to the normal clock.

The brightness choice is saved as it is changed. The boot mode is saved only
when **Remember** is selected.

### Scheduled Night Mode

Night mode is configured on the two Night pages in **Configuration**:

1. Enable the schedule and select the hours to start dimming and return to
   normal brightness.
2. Choose **Dim only** to keep the display at its lowest visible brightness,
   or **Screen off** and select the hour when the backlight should turn off.

The screen-off hour must fall inside the configured night interval. The clock,
timers, and alarms continue running while the backlight is off. A touchscreen
press, the discrete touch sensor, or either physical button restores full
brightness for 10 seconds. The first Clock or Alarm press while night mode is
dimmed or off is used only to wake the display; press it again to open its
screen. A ringing alarm or finished timer also restores the normal display.

### Hourly Chime

The four Chime pages in **Configuration** configure:

- **Off**, **Hourly**, or **Quarter hour** playback. Quarter-hour mode plays
  at `:00`, `:15`, `:30`, and `:45`.
- Any `.mp3` file stored in LittleFS. Select a file and tap **Play** to
  preview it at a reduced, distortion-safe volume.
- 25%, 50%, 75%, or 100% volume.
- Optional quiet hours, including schedules that cross midnight.

Chime settings are saved immediately. A due chime is skipped during quiet
hours or while an alarm, timer, startup effect, or other sound is already
playing.

### Optional Wi-Fi Mode

Wi-Fi is disabled by default, and the RTC, alarms, timers, night mode, and
local weather sensor continue to work without a network.

To configure it:

1. Open the **Wi-Fi** page in Configuration and choose **Setup Wi-Fi**.
2. Scan the on-screen QR code with an iPhone or Android camera to join the
   open `Maclock Setup` access point, or connect to it manually.
3. Open `http://192.168.4.1` if the setup page does not appear automatically.
4. Enter the Wi-Fi name, password, and city, then save.
5. Return to Maclock and press **Back**.

When enabled and connected, Maclock:

- synchronizes the external RTC from NTP;
- obtains the city coordinates and automatic UTC/DST offset;
- refreshes the current conditions, daily low/high, and rain probability;
- serves a local phone-friendly control panel;
- pauses its Wi-Fi worker while Mini vMac is running.

The clock switches back to its local BMP5xx/HTU2x display if the online
forecast becomes stale or cannot be reached. The last synchronized RTC time
continues normally while offline. A DST change that happens during a long
offline period is applied after the next successful connection.

City lookup and weather use the
[Open-Meteo Geocoding API](https://open-meteo.com/en/docs/geocoding-api) and
[Forecast API](https://open-meteo.com/en/docs). Wi-Fi credentials and the city
are stored in the device's persistent settings.

### Web Control Panel

The control panel is separate from the `Maclock Setup` captive portal. Setup
only configures the network, city, timezone, and weather connection. Once
Maclock is connected to the configured Wi-Fi, open:

- `http://maclock.local/`; or
- `http://<device-ip>/`, using the address shown in Diagnostics.

The responsive control panel can:

- change the clock face, light/dark theme, accent, numeral size, weather,
  Flip speed, brightness, 12/24-hour format, leading zero, weekday, and
  seconds immediately;
- configure and persist all three alarms, including weekly days, sound, and
  volume;
- save timer defaults, start or cancel the timer, and select its sound and
  volume;
- configure night-mode dimming and screen-off hours;
- configure hourly/quarter-hour chimes and quiet hours;
- choose persistent startup and floppy sounds and their volumes;
- upload or import MP3 files into protected `/downloaded/` storage, preview
  built-in and downloaded sounds, and remove downloaded sounds explicitly;
- discover stable GitHub releases, reconcile release assets, install verified
  HTTPS firmware updates, upload a Maclock ESP32-S3 `.bin`, and reboot.

The panel is served directly by Maclock and uses no cloud service. It is
available only while the clock is connected to the same local network and is
stopped before the Wi-Fi setup portal or Mini vMac takes ownership.

The interface is a responsive Vue application styled after classic Macintosh
control panels. Its editable source is in `web/control-panel/`; the generated
`src/control_panel_page.h` must not be edited by hand. During every PlatformIO
build, `scripts/build_control_panel.py` checks a source fingerprint and
automatically runs the Vue build when the embedded header is stale. The final
single-file HTML document is gzip-compressed before it is placed in firmware.

For web-only development, run:

```sh
cd web/control-panel
npm install
npm run dev
```

The development server uses simulated Maclock state, so controls and timer
updates can be tested without hardware. `npm run build` performs the same
single-file build and header generation used by PlatformIO.

### Software Updates And Downloaded Sounds

The ninth web application, **Software Update**, checks the latest stable
release at `github.com/fensoft/maclock`. Maclock also checks after Wi-Fi
connects and every 24 hours. A new release can be installed from the device
prompt or web panel, deferred until later, or ignored by version. Prompts wait
until alarms, chimes, audio playback, calibration, and Mini vMac are inactive.

Official updates download the release manifest, reconcile its tracked
LittleFS files one at a time, then write verified firmware to the inactive OTA
slot. Each changed file is streamed through a 32 KiB DEFLATE dictionary,
hashed, and atomically renamed; the complete ZIP and firmware SHA-256 values
must also match. Interrupted asset updates resume after Wi-Fi reconnects.

Sounds uploaded with drag-and-drop or a file picker, imported by URL, or
downloaded from MyInstants are stored below `/downloaded/`. This directory is
never created, overwritten, renamed, reclaimed, or deleted by OTA. The Sound
Manager file browser therefore shows only downloaded MP3s, while every sound
selector continues to show built-in and downloaded sounds. If those files
leave too little working space, the update stops and asks you to delete sounds
deliberately through Sound Manager.

The raw firmware upload control updates firmware only. It accepts a structural
Maclock ESP32-S3 application image but cannot update LittleFS assets.

Version 1.0.0 changes the flash partition table and therefore requires one
final USB installation of the bootloader, partition table, firmware, and
LittleFS image. This first repartition is destructive: existing MP3s, ROMs,
disk images, and every other LittleFS file are erased. NVS preferences remain
unless a separate factory reset is performed. Afterward, files placed in
`/downloaded/` survive normal OTA updates.

### Calibrating The Touchscreen

Calibration is entered from Configuration:

1. Open **Configuration** as described above.
2. Release the Clock button.
3. Press the **Clock** button again.
4. Touch and release each crosshair in order:
   top-left, top-right, bottom-right, and bottom-left.

After the fourth point, the calibration is saved and Maclock opens the normal
clock display. Press **Clock** during calibration to cancel and return to Boot
Configuration without changing the saved calibration.

<p align="center">
  <img src="img/config_calib.jpg" alt="Maclock touchscreen calibration crosshair" width="640">
</p>

### Clock Startup

Clock mode follows a Macintosh-style startup sequence:

1. The background appears and `startup.mp3` plays.
2. Missing-disk images alternate while Maclock waits for the floppy switch.
3. Activating the switch plays `floppy.mp3`.
4. Maclock checks the expected I²C plugins and reveals their icons.
5. If all required devices are present, the normal clock appears.

The startup diagnostic expects:

| Address | Device |
| --- | --- |
| `0x18` | ES8311 audio codec |
| `0x38` | FT6336 touchscreen |
| `0x40`, `0x47`, or `0x50` | HTU2x or BMP580/BMP581 weather sensor |
| `0x68` | DS1307 or DS3231 real-time clock |

| Successful plugin detection | Missing-plugin diagnostic |
| --- | --- |
| ![Boot screen showing all plugin icons](img/clock_booting.jpg) | ![Boot screen showing a red missing-plugin icon](img/clock_booting_error.jpg) |

A missing expected device is shown in red and blinks continuously. This is a
deliberate diagnostic stop; correct the connection and restart Maclock. Serial
output at 115200 baud reports RTC and weather-sensor detection details.

### Using The Clock

The main screen displays:

- Time and date from the external RTC.
- When online, current temperature, daily low/high, rain probability, and a
  forecast icon.
- When offline, temperature plus a weather icon and gauge derived from the
  detected pressure or humidity sensor.
- A small floppy icon while the floppy switch is active.

When a BMP580/BMP581 is installed, the gauge represents atmospheric pressure.
When an HTU2x is installed, it represents relative humidity. If both are
connected, the BMP5xx is preferred.

Rotate the encoder to change the backlight. The selected value is saved after a
short delay. Touching the discrete red-wire sensor temporarily sets the
backlight to maximum.

Touching the display shows a Macintosh pointer, which automatically disappears
after two seconds.

<p align="center">
  <img src="img/final_front_floppy.jpg" alt="Clock interface with the floppy inserted" width="640">
</p>

### Setting Alarms

Press the **Alarm** button from the main clock screen to configure up to three
alarms. Each alarm has its own:

- Enabled setting.
- Hour and minute.
- Monday-through-Sunday schedule.
- Sound: any `.mp3` file stored in LittleFS, with a **Play** button for
  previewing it at a reduced, distortion-safe volume.
- Volume: 25%, 50%, 75%, or 100%.

The first page has large square **Alarm** and **Timer** buttons.
Alarm opens six pages with large touch targets: alarm selection, time, days,
sound, volume, and actions. Every page has fixed **Previous**, **Exit**, and
**Next** footer slots, leaving an empty direction on the first and last pages.
The four time buttons always adjust the selected alarm by exactly one hour or
one minute. Tap **Save** on the last page to store all three alarms
persistently. **Exit** discards changes made since opening the editor.

An alarm-clock icon appears to the left of the date when at least one alarm is
enabled or snoozed. When an alarm rings, press **Alarm**, touch the discrete
red-wire sensor, or tap **Snooze 9 min** to snooze it. Press **Clock** or tap
**Dismiss** to stop it for the current occurrence.

### Using The Timer

1. Press **Alarm** from the main clock screen.
2. Tap **Timer** on the first page.
3. Adjust the duration with **-10**, **-1**, **+1**, and **+10**.
4. Tap **Start**.

The duration is limited to 1–99 minutes.

The timer returns to the normal clock and continues counting down in the
background. While it is active, its remaining time replaces the date on the
main clock. Opening the timer dialog again shows the live countdown and
provides **Stop**, **Start** to restart with the selected duration, and
**Back**.

When the countdown reaches zero, a Macintosh-style completion dialog appears
and the timer sound repeats. Tap **Dismiss**, or press either **Clock** or
**Alarm**, to return to the normal clock.

### Setting The Date And Time

1. Press **Clock** from the main screen.
2. Open the **Date / Time** page. Its hour, minute, second, day, month, and
   year buttons refresh live from the RTC.
3. Tap the field to change.
4. Use **-** and **+** to write the new value directly to the RTC. Hold a
   button to repeat.

<p align="center">
  <img src="img/config_date.jpg" alt="Maclock date and time editor" width="640">
</p>

Supported RTCs are DS1307 and DS3231 at I²C address `0x68`. Without a working
RTC, the firmware falls back to `01/01/2000 00:00:00`, and the startup plugin
diagnostic will not complete. Configuration and Diagnostics also warn when a
DS1307 is stopped, a DS3231 reports lost power, the date is invalid, or the
year is earlier than 2024. A healthy RTC does not add a status line to Boot
Configuration. If the date returns to 2000 after unplugging Maclock, check or replace
the RTC backup battery, then set the date again.

### Using Mini vMac

Mini vMac mode emulates a Macintosh Plus with a 304x224 monochrome desktop
inside the physical 320x240 display.

| About This Macintosh | MacPaint |
| --- | --- |
| ![Mini vMac About This Macintosh dialog](img/emulator_about.jpg) | ![MacPaint running in Mini vMac](img/emulator_paint.jpg) |

The LittleFS image must contain:

- `vMac.ROM`, a compatible 128 KiB Macintosh Plus ROM.
- `disk1.dsk`, the first bootable disk image.
- Optional sequential images named `disk2.dsk`, `disk3.dsk`, and so on.

Maclock stops looking for disks at the first missing number and supports up to
six mounted images. For example, `disk1.dsk` and `disk3.dsk` without
`disk2.dsk` will mount only `disk1.dsk`.

To start the emulator, open Boot Options and tap **Start Emulator**. To make it
the normal power-on mode, select **Remember** before starting it. The physical
floppy switch can be either active or inactive.

Move a finger on the display to move the Macintosh pointer. Use the discrete
red-wire touch sensor as the mouse button. In the emulator, the **Clock** button
acts as **Enter**, the **Alarm** button acts as **Escape**, and the rotary
encoder adjusts and saves the display brightness. A control reminder is shown
over the emulated screen for four seconds after startup.

Hold **Clock** and **Alarm** together for two seconds to safely eject and close
the mounted disk images, stop Mini vMac, and return to Boot Options. Macintosh
mono sound plays through the ES8311 speaker output.

Disk images are opened read/write when possible, so changes made inside the
emulated Macintosh persist in LittleFS. Keep backup copies of important disk
images. Uploading a new LittleFS image replaces them; release reconciliation
also removes files not owned by the release except for `/downloaded/`.

### Adding Macintosh Software

1. In Infinite Mac, open
   [System 7.0 at 320x240](https://infinitemac.org/1991/System%207.0?screenSize=320x240)
   and copy the desired software to the **Saved HD** drive.
2. Export the Saved HD image from the Infinite Mac settings.
3. Download a blank disk image from the
   [Mini vMac blanks page](https://www.gryphel.com/c/minivmac/extras/blanks/).
4. Open the blank image in
   [Mini vMac](https://www.gryphel.com/c/minivmac/download.html), then open the
   exported Infinite Mac disk image.
5. Copy the software from the exported disk to the blank disk.
6. Place the resulting image in `data/` as the next contiguous name, such as
   `disk2.dsk`.
7. Rebuild and upload LittleFS using the instructions in
   [BUILD.md](BUILD.md#build-and-upload).

Use only ROMs and software that you are legally entitled to use and distribute.

## Troubleshooting

### The missing-disk images keep alternating

The clock startup is waiting for the floppy switch. Check its GPIO 47 wiring
and activate the switch.

### A red plugin icon blinks forever

An expected I²C device did not answer. Check power, ground, SDA/SCL wiring, and
the addresses listed in the startup table. Use the 115200-baud serial monitor
for RTC and weather detection messages.

### Touches land in the wrong place

Repeat the four-point calibration from Boot Options.

### Mini vMac does not start

Confirm all of the following:

- Start it from **Boot Options**, or save Emulator as the default using
  **Remember**.
- `vMac.ROM` and `disk1.dsk` were included in the uploaded LittleFS image.
- The ROM is a compatible 128 KiB Macintosh Plus ROM.

### Images or sounds are missing

Build and upload the filesystem image. A firmware-only upload does not install
files from `data/`.

### The date resets to 2000

The RTC was not detected or initialized. Check the device, its backup battery,
I²C wiring, and address `0x68`.

## Developer Documentation

- [BUILD.md](BUILD.md): hardware construction, firmware preparation, and
  flashing.
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md): firmware architecture.
- [AGENTS.md](AGENTS.md): repository-specific development guidance.
