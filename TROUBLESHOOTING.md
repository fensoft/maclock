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

## The date resets to 2000

The RTC was not detected, initialized, or retained time. Check the RTC, backup
battery, I²C wiring, and address `0x68`, then set the date again.

## The web control panel does not open

Confirm Wi-Fi is connected, use the IP shown in Diagnostics, and ensure the
phone or computer is on the same local network. The panel is unavailable
during Wi-Fi setup and Mini vMac operation.
