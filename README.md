# Maclock

Maclock is a hardware-hacking project that replaces the original Maclock
screen with a 320x240 color IPS display driven by an ESP32-S3.

The firmware has two operating modes:

- **Clock mode** presents a Macintosh-inspired clock with startup sounds,
  date/time editing, weather data, touch calibration, and adjustable
  brightness.
- **Mini vMac mode** emulates a Macintosh Plus from ROM and disk images stored
  in LittleFS.

## Discord

Join the community on the [Discord server](https://discord.gg/89etSPMFym).

## Recommended Hardware

Use the
[2.8inch ESP32-S3 Display](https://www.lcdwiki.com/2.8inch_ESP32-S3_Display).

## Software Manual

### Controls

| Control | Clock mode | Mini vMac mode |
| --- | --- | --- |
| Rotary encoder | Adjust brightness from off to maximum | Not used |
| Clock button | Open the date/time editor | Not used |
| Alarm button | Reserved; no software action yet | Not used |
| Floppy switch | Advance startup and show the floppy icon | Select emulator boot when active at power-on |
| Display touchscreen | Operate menus and show the pointer | Move the Macintosh pointer using relative motion |
| Discrete touch sensor (red wire) | Force full brightness for 10 seconds | Macintosh mouse button |

Brightness and boot choices are saved and restored after power loss. Touchscreen
calibration is also stored persistently.

### Choosing The Boot Mode

The saved **Boot with floppy** option and the physical floppy switch determine
which software starts.

- **Emulator**: if the floppy switch is active during power-on, Maclock starts
  Mini vMac immediately.
- **Clock**: Maclock always starts the clock interface, even when the floppy
  switch is active.
- If the switch is not active at power-on, Maclock starts the clock interface
  with either setting.

To open the boot-options screen:

1. Turn Maclock off.
2. Hold the **Clock** button.
3. Turn Maclock on while continuing to hold the button.
4. Release the button when **Boot Options** appears.

The screen provides these choices:

- **Brightness / Latest** restores the last encoder brightness.
- **Brightness / Lowest** starts at the lowest visible setting.
- **Brightness / Highest** starts at full brightness.
- **Boot with floppy / Emulator** enables the power-on Mini vMac shortcut.
- **Boot with floppy / Clock** keeps the device in clock mode.

Selections are saved as they are changed. Tap **Continue** to run the normal
clock startup.

### Calibrating The Touchscreen

Calibration is entered from the boot-options screen:

1. Open **Boot Options** as described above.
2. Release the Clock button.
3. Press the **Clock** button again.
4. Touch and release each crosshair in order:
   top-left, top-right, bottom-right, and bottom-left.

After the fourth point, the calibration is saved and Maclock opens the normal
clock display.

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
| `0x40` or `0x47` | HTU2x or BMP580/BMP581 weather sensor |
| `0x68` | DS1307 or DS3231 real-time clock |

A missing expected device is shown in red and blinks continuously. This is a
deliberate diagnostic stop; correct the connection and restart Maclock. Serial
output at 115200 baud reports RTC and weather-sensor detection details.

### Using The Clock

The main screen displays:

- Time and date from the external RTC.
- Temperature from the detected weather sensor.
- A weather icon and gauge derived from pressure or humidity.
- A small floppy icon while the floppy switch is active.

When a BMP580/BMP581 is installed, the gauge represents atmospheric pressure.
When an HTU2x is installed, it represents relative humidity. If both are
connected, the BMP5xx is preferred.

Rotate the encoder to change the backlight. The selected value is saved after a
short delay. Touching the discrete red-wire sensor temporarily sets the
backlight to maximum.

Touching the display shows a Macintosh pointer, which automatically disappears
after two seconds.

### Setting The Date And Time

1. Press the **Clock** button from the main screen.
2. Tap the hour, minute, second, day, month, or year field.
3. Use **-** and **+** to change the selected value. Hold a button to repeat.
4. Tap **Save** to write the value to the RTC, or **Cancel** to discard it.

Supported RTCs are DS1307 and DS3231 at I²C address `0x68`. Without a working
RTC, the firmware falls back to `01/01/2000 00:00:00`, and the startup plugin
diagnostic will not complete.

### Using Mini vMac

Mini vMac mode emulates a Macintosh Plus with a 304x224 monochrome desktop
inside the physical 320x240 display.

The LittleFS image must contain:

- `vMac.ROM`, a compatible 128 KiB Macintosh Plus ROM.
- `disk1.dsk`, the first bootable disk image.
- Optional sequential images named `disk2.dsk`, `disk3.dsk`, and so on.

Maclock stops looking for disks at the first missing number and supports up to
six mounted images. For example, `disk1.dsk` and `disk3.dsk` without
`disk2.dsk` will mount only `disk1.dsk`.

To start the emulator:

1. Select **Boot with floppy / Emulator** in Boot Options.
2. Turn Maclock off.
3. Activate the floppy switch.
4. Turn Maclock on.

Move a finger on the display to move the Macintosh pointer. Use the discrete
red-wire touch sensor as the mouse button.

Emulator sound is currently disabled. There is no software command to return
from Mini vMac to the clock; power-cycle Maclock and leave the floppy switch
inactive, or select **Clock** from Boot Options.

Disk images are opened read/write when possible, so changes made inside the
emulated Macintosh persist in LittleFS. Keep backup copies of important disk
images. Uploading a new LittleFS image can replace the on-device disks and
their changes.

## Hardware Assembly

- Separate the LCD from the main board with a knife.
- Desolder it so a roughly 10 cm wire extension can be added before
  reconnecting it.
- All components connect to **GND** and their respective GPIO pins, except as
  required by the touch-sensor wiring.

| Component | GPIO |
| --- | --- |
| Encoder CLK | GPIO 14 |
| Encoder DT | GPIO 21 |
| I²C SDA | GPIO 16 |
| I²C SCL | GPIO 15 |
| Touch Sensor (red wire) | GPIO 2 |
| Floppy Switch | GPIO 47 (SD connector pad 2) |
| Alarm Button | GPIO 40 (SD connector pad 3) |
| Clock Button | GPIO 48 (SD connector pad 1, exterior) |

The I²C bus is shared with the onboard ES8311 audio codec (`0x18`) and FT6336G
touch controller (`0x38`). The firmware also supports these external
3.3 V-compatible I²C devices:

- HTU2x temperature/humidity sensor at `0x40`.
- BMP580/BMP581 pressure/temperature sensor at `0x47`.
- DS1307 or DS3231 real-time clock at `0x68`.

## Firmware Installation

1. Install Visual Studio Code.
2. Install the PlatformIO IDE extension.
3. Open this folder in Visual Studio Code and let PlatformIO initialize the
   environment.
4. Run `./prepare.sh` once in a fresh checkout to prepare Mini vMac and the
   initial emulator files.
5. Build and upload the firmware.
6. Upload the filesystem image so assets and emulator files under `data/` are
   flashed to LittleFS.

In PlatformIO:

- **PlatformIO: Upload** uploads the firmware.
- **PlatformIO: Upload Filesystem Image** uploads LittleFS.

Equivalent command-line builds are:

```bash
pio run -e lolin_s3
pio run -e lolin_s3 -t buildfs
```

If `pio` is not on `PATH`, use `~/.platformio/penv/bin/pio`.

Firmware and filesystem uploads are separate. Uploading only the firmware does
not update images, audio, ROMs, or disks. Uploading the filesystem may overwrite
mutable emulator disks, so back them up first.

## Adding Macintosh Software

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
7. Rebuild and upload the LittleFS image.

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

- **Boot with floppy** is set to **Emulator**.
- The floppy switch is already active when power is applied.
- `vMac.ROM` and `disk1.dsk` were included in the uploaded LittleFS image.
- The ROM is a compatible 128 KiB Macintosh Plus ROM.

### Images or sounds are missing

Build and upload the filesystem image. A firmware-only upload does not install
files from `data/`.

### The date resets to 2000

The RTC was not detected or initialized. Check the device, its backup battery,
I²C wiring, and address `0x68`.

## Developer Documentation

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the firmware architecture
and [AGENTS.md](AGENTS.md) for repository-specific development guidance.
