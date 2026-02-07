# Maclock

A hardware hacking project replacing the Maclock screen with a color IPS display driven by ESP32.

## Recommended Hardware

Use [2.8inch_ESP32-S3_Display](https://www.lcdwiki.com/2.8inch_ESP32-S3_Display)

## Discord

Join our community: [Discord Server](https://discord.gg/89etSPMFym)

## Building

- Separate the LCD from the main board with a knife
- Desolder it so you can add a ~10 cm wire extension before reconnecting
- All component connect to **GND** and their respective GPIO pins (except touch sensor)

| Component | GPIO |
|-----------|------|
| Encoder CLK | GPIO 14 |
| Encoder DT | GPIO 21 |
| Encoder Center | GPIO 8 |
| Touch Sensor (red wire) | GPIO 2 |
| Floppy Switch | GPIO 47 (sd connector) |
| Alarm Button | GPIO 40 (sd connector) |
| Clock Button | GPIO 48 (sd connector) |

## Installation

1. Install Visual Studio Code.
2. Install PlatformIO IDE (VS Code extension).
3. Open this folder in VS Code and let PlatformIO initialize the environment.
4. Build and upload firmware.
5. Upload the filesystem image (LittleFS) so assets in `data/` are flashed.

In PlatformIO:
- `PlatformIO: Upload` (firmware)
- `PlatformIO: Upload Filesystem Image` (LittleFS)
