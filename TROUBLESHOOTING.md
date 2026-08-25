# Maclock Troubleshooting

Use this guide when an assembled Maclock does not start or behave as expected.
For normal operation, see the [User Manual](manual.md).

## Hardware diagnostics

Open **System > Tools > Diagnostics** to view Clock, Alarm, floppy, encoder,
touch, charging, I²C, Wi-Fi, and RTC status.

| Tools | About |
| --- | --- |
| ![System Tools page](img/manual/tools.png) | ![Maclock About page](img/manual/about.png) |

![Hardware Diagnostics](img/manual/diagnostics.png)

## Startup diagnostics

Clock startup expects:

| Address | Device |
| --- | --- |
| `0x18` | ES8311 audio codec |
| `0x38` | FT6336 touchscreen |
| `0x40`, `0x47`, or `0x50` | HTU2x or BMP580/BMP581 weather sensor |
| `0x68` | DS1307 or DS3231 real-time clock |

| Successful detection | Missing-device diagnostic |
| --- | --- |
| ![All plugin icons detected](img/clock_booting.jpg) | ![Red missing-plugin icon](img/clock_booting_error.jpg) |

A missing device appears red and blinks continuously. This is a deliberate
stop: correct power, ground, SDA/SCL wiring, and device address, then restart.
The 115200-baud serial output reports detection details.

## Missing-disk pictures alternate forever

Clock startup is waiting for the floppy lever. Check its connection and
activate it.

## A red plugin icon blinks forever

An expected I²C device did not respond. Check power, ground, SDA/SCL, and the
addresses in [Startup diagnostics](#startup-diagnostics). Correct the failed
connection and restart Maclock.

## Touches land in the wrong place

Repeat the
[four-point touchscreen calibration](manual.md#calibrate-the-touchscreen).

## Mini vMac does not start

Confirm that:

- Emulator was selected from **System > Start**.
- `vMac.ROM` is a compatible 128 KiB Macintosh Plus ROM.
- `disk1.dsk` is present.
- Disk image names are contiguous: `disk1.dsk`, `disk2.dsk`, and so on.

## Images or sounds are missing

Install the LittleFS image. A firmware-only upload does not install files from
`data/`.

Run `node scripts/audit_littlefs_assets.mjs` before rebuilding LittleFS. It
reports missing static, weather, and I2C module project assets without deleting
anything. PlatformIO package-install messages can be noisy on a fresh machine;
the relevant result is the final build status.

## The loading screen falls back to the legacy boot view

The selected loading project could not be loaded. Confirm that
`/loading/<id>/loading.json` uses the `maclock-loading-screen` version-1 format
at 304x224, and that every referenced PNG is in the same project directory.
Supported static layers are rectangles, circles, lines, text, and images.

For dynamic I2C modules, use one image with `id` `module`, template
`plugin_{i2c}.png`, and `next_module_x`/`next_module_y` spacing. Provide the
project-local `plugin_0x18.png`, `plugin_0x38.png`, weather-address, and
`plugin_0x68.png` assets as needed; unavailable hardware is shown in red. A
project sound must be an existing absolute LittleFS path without `..`, with a
valid volume. Rebuild and upload LittleFS after changing source `data/`.

If the selected project is missing or invalid, Maclock tries the first valid
loading project and then the legacy boot view.

## The Loading Screen Editor list is empty in the simulator

The desktop overlay may contain deletion markers from an older state. Close the
simulator and restart it with `--reset-state` to remove only its Preferences,
EEPROM, and LittleFS overlay state. Recreated directories also clear stale
markers automatically, so built-in projects are enumerated again.

## Localized custom-face text is clipped or shows a bad degree unit

Check each custom project's `{tr.*}` entry for the active language and leave an
appropriate width for longer translations. Use the supported degree text emitted
by `{temperature_unit}` rather than substituting an unsupported glyph. Changing
**Show Seconds** or the 12/24-hour setting rebuilds the active custom face so
conditional and time-dependent content can update.

## Backing up before filesystem changes

Use the control panel's configuration export/import workflow and verify that an
export can be imported successfully. Archives include settings, loading-screen
projects and assets, downloaded media, Mini vMac ROM, and root disk images.
Still make a separate host backup of emulator disks before filesystem upload,
because a new LittleFS image can replace mutable media.

## The date resets to 2000

The RTC was not detected, initialized, or retained time. Check the RTC, backup
battery, I²C wiring, and address `0x68`, then set the date again.

## The web control panel does not open

Confirm Wi-Fi is connected, use the IP shown in Diagnostics, and ensure the
phone or computer is on the same local network. The panel is unavailable
during Wi-Fi setup and Mini vMac operation.
