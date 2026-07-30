# Maclock User Manual

This manual explains how to operate a completed Maclock. For assembly, wiring,
and initial firmware installation, use the illustrated
[Build Your Own guide](BUILD.md).

## 1. Know Your Maclock

1. **Discrete touch** — located on top of Maclock.
2. **Touchscreen** — tap controls and move the Macintosh pointer.
3. **Rotary encoder** — located at the bottom left; rotate it to adjust
   display brightness.
4. **Clock and Alarm** — the two buttons at the bottom right.
5. **Floppy lever** — reports whether the physical floppy is inserted.

### Control reference

| Control | Clock mode | Mini vMac mode |
| --- | --- | --- |
| Rotary encoder | Adjust brightness from off to maximum | Adjust brightness from off to maximum |
| Clock | Open Configuration; dismiss an alarm; wake night mode | Enter |
| Alarm | Open alarms and timers; snooze an alarm; wake night mode | Escape |
| Clock + Alarm, held 2 seconds | Open Configuration | Safely exit to Configuration |
| Floppy lever | Continue startup and show the floppy icon | No action |
| Touchscreen | Operate menus, show the pointer, and wake the display | Move the Macintosh pointer |
| Discrete touch | Full brightness for 10 seconds; snooze an alarm | Macintosh mouse button |

Brightness uses perceptual steps, so lower levels change more gradually.

## 2. Important Information

> [!CAUTION]
> Mini vMac disks are writable. Always leave the emulator with the
> **Clock + Alarm** safe-exit chord before removing power. Keep backup copies
> of important disk images.

A firmware-only USB upload does not install images, sounds, ROMs, or disks.
Upload the LittleFS image when those files change. Normal verified updates
preserve files in `/downloaded/`.

## 3. Quick Start

1. Connect USB power.
2. If Clock mode is selected, wait for the alternating floppy symbols.
3. Insert the floppy.
4. Wait while Maclock checks its touchscreen, audio codec, weather sensor,
   and real-time clock.
5. The normal Macintosh clock appears when all required devices respond.

| Configuration | Normal Macintosh clock |
| --- | --- |
| ![Configuration section hub](img/manual/configuration-hub.png) | ![Normal Macintosh clock face](img/manual/clock-macintosh.png) |

If a red plugin icon continues blinking, see
[Startup diagnostics](TROUBLESHOOTING.md#startup-diagnostics).

## 4. Choose the Startup Mode

Maclock can start in Clock or Mini vMac mode regardless of the floppy presence.
The saved default determines what happens at power-on.

To open Configuration:

1. Press **Clock** from the normal clock; or
2. Hold **Clock** while connecting power, then release it when Configuration
   appears.

Configuration contains four sections:

- **General** — Language, Regional, and Date / Time.
- **Display** — display content, faces, styling, screensaver, and night mode.
- **Sound** — chime schedule, sound, volume, and quiet hours.
- **System** — preferences, startup, Wi-Fi, tools, updates, and About.

Tap **Sections** to return to the hub. Tap **Exit** on the hub to return to
the clock.

### Select Clock or Mini vMac

1. Open **System**.
2. Go to **Start**.
3. Choose **Clock** or **Emulator**.
4. Choose **Remember** to make it the power-on default, or **One time** to use
   it only now.

| Preferences | Startup mode |
| --- | --- |
| ![Brightness startup preferences](img/manual/preferences.png) | ![Clock or emulator startup selection](img/manual/start-mode.png) |

Brightness can start at the latest saved level, the lowest visible level, or
full brightness. Brightness changes are saved immediately; the startup mode is
saved only when **Remember** is selected.

## 5. General Settings

### Language

Open **General > Language**, then choose English, Français, Español, Deutsch,
or Italiano. The interface changes immediately and remembers the selection.

![Language selection](img/manual/language.png)

### Regional format

Open **General > Regional** to choose:

- Month/day/year, day/month/year, or year/month/day.
- 12-hour or 24-hour time.
- Celsius or Fahrenheit.

![Regional date, time, and temperature choices](img/manual/regional.png)

### Set the date and time

1. Open **General > Date / Time**.
2. Tap hour, minute, second, day, month, or year.
3. Tap **-** or **+**. Hold a button to repeat.

![Date and time editor](img/manual/date-time.png)

## 6. Display and Clock Faces

### Display content

Open **Display > Display** to control:

- Leading zero.
- Localized three-letter weekday.
- Seconds.
- Light or dark theme.

Hiding seconds also removes the Analog second hand.

![Basic display options](img/manual/display.png)

### Choose and style a face

Open **Display > Clock Face** to select a face. Macintosh and Mac OS 8 use
full-screen Macintosh artwork; Compact, Analog, Flip, and Odometer use their
own layouts.

### Display modes

| Macintosh | Compact |
| --- | --- |
| ![Macintosh display mode](img/manual/clock-macintosh.png) | ![Compact display mode](img/manual/clock-compact.png) |

| Analog | Flip |
| --- | --- |
| ![Analog display mode](img/manual/clock-analog.png) | ![Flip display mode](img/manual/clock-flip.png) |

| Odometer | Mac OS 8 |
| --- | --- |
| ![Odometer display mode](img/manual/clock-odometer.png) | ![Mac OS 8 display mode](img/manual/clock-macos8.png) |

| Clock face | Face style | Face details |
| --- | --- | --- |
| ![Clock face choices](img/manual/clock-face.png) | ![Accent and numeral size](img/manual/face-style.png) | ![Weather and animation detail](img/manual/face-details.png) |

**Face Style** selects Default, red, orange, green, blue, or purple accents
and Small, Default, or Large numerals. **Face Details** controls weather on
supported faces and the Flip animation speed.

### Screensaver

Use **Display > Screensaver** to choose the inactivity behavior.

![Screensaver settings](img/manual/screensaver.png)

The available screensavers are:

| After Dark | Starfield |
| --- | --- |
| ![After Dark screensaver with a floating digital clock](img/manual/screensaver-after-dark.png) | ![Starfield screensaver](img/manual/screensaver-starfield.png) |

| Bouncing Mac | Matrix Rain |
| --- | --- |
| ![Bouncing Mac screensaver](img/manual/screensaver-bouncing-mac.png) | ![Matrix Rain screensaver](img/manual/screensaver-matrix-rain.png) |

| Pipes | Flying Clocks |
| --- | --- |
| ![Pipes screensaver](img/manual/screensaver-pipes.png) | ![Flying Clocks screensaver](img/manual/screensaver-flying-clocks.png) |

**Random** selects one of the six animated screensavers and periodically
changes it. **Off** disables automatic screensaver activation.

### Scheduled night mode

1. Open the first **Night** page.
2. Enable the schedule and choose when dimming begins and ends.
3. Open the second **Night** page.
4. Choose **Dim only** or **Screen off**.
5. For Screen off, choose an hour inside the configured night interval.

| Night schedule | Night behavior |
| --- | --- |
| ![Night mode schedule](img/manual/night-schedule.png) | ![Night mode dim or screen-off behavior](img/manual/night-behavior.png) |

The clock, alarms, and timers continue while the backlight is off. A
touchscreen press, discrete touch, Clock, or Alarm restores full brightness
for 10 seconds. The first Clock or Alarm press while dimmed is consumed by
waking the screen; press again for the normal action. A ringing alarm or
finished timer also restores the display.

## 7. Alarms, Timers, and Chimes

### Set an alarm

1. Press **Alarm** on the normal clock.
2. Tap **Alarm**.
3. Select one of the three alarms.
4. Set its hour and minute.
5. Select Monday through Sunday.
6. Choose any available MP3 sound and optionally preview it.
7. Choose 25%, 50%, 75%, or 100% volume.
8. Tap **Save** on the final page.

| Alarm or Timer | Select alarm | Set time |
| --- | --- | --- |
| ![Alarm and timer chooser](img/manual/alarm-timer-home.png) | ![Alarm selection](img/manual/alarm-selection.png) | ![Alarm time controls](img/manual/alarm-time.png) |

| Repeat days | Sound | Volume |
| --- | --- | --- |
| ![Weekly alarm days](img/manual/alarm-days.png) | ![Alarm sound selection](img/manual/alarm-sound.png) | ![Alarm volume](img/manual/alarm-volume.png) |

![Alarm save and cancel actions](img/manual/alarm-actions.png)

The four time buttons change the selected alarm by exactly one hour or one
minute. **Exit** discards changes made since opening the editor.

When at least one alarm is enabled or snoozed, an alarm-clock icon appears
beside the date. When an alarm rings:

- Press **Alarm**, use discrete touch, or tap **Snooze 9 min** to snooze.
- Press **Clock** or tap **Dismiss** to stop the current occurrence.

### Use the timer

1. Press **Alarm** from the clock.
2. Tap **Timer**.
3. Adjust the duration with **-10**, **-1**, **+1**, and **+10**.
4. Tap **Start**.

![Timer duration editor](img/manual/timer-editor.png)

The duration is limited to 1–99 minutes. The timer continues in the
background and replaces the date with its remaining time. Reopen Timer to
stop it or restart it with the selected duration. At zero, the completion
sound repeats until you tap **Dismiss** or press Clock or Alarm.

### Configure the chime

Open the four pages under **Sound**:

1. Choose **Off**, **Hourly**, or **Quarter hour**. Quarter-hour mode plays at
   `:00`, `:15`, `:30`, and `:45`.
2. Choose an MP3 and use **Play** to preview it.
3. Choose 25%, 50%, 75%, or 100% volume.
4. Optionally configure quiet hours, including schedules across midnight.

| Schedule | Sound | Volume | Quiet hours |
| --- | --- | --- | --- |
| ![Chime schedule](img/manual/chime-mode.png) | ![Chime sound](img/manual/chime-sound.png) | ![Chime volume](img/manual/chime-volume.png) | ![Chime quiet hours](img/manual/quiet-hours.png) |

A due chime is skipped during quiet hours or while another alarm, timer,
startup effect, or sound is playing.

## 8. Wi-Fi and Weather

Wi-Fi is optional. The RTC, alarms, timers, night mode, and local weather
sensor continue without a network.

### Connect to Wi-Fi

1. Open **System > Wi-Fi**.
2. Choose **Setup Wi-Fi**.
3. Scan the displayed QR code to join the open `Maclock Setup` network, or
   join it manually.
4. If the setup page does not open, browse to `http://192.168.4.1`.
5. Enter the network name, password, and city, then save.
6. Return to Maclock and tap **Back**.

| Wi-Fi settings | Setup QR code |
| --- | --- |
| ![Wi-Fi settings](img/manual/wifi.png) | ![Wi-Fi Setup QR code and connection instructions](img/manual/wifi-setup-qr.png) |

When connected, Maclock synchronizes the RTC, obtains city coordinates and
automatic UTC/DST offset, retrieves current conditions and forecasts, and
serves its local control panel. If online data becomes stale, the clock falls
back to the local BMP5xx or HTU2x sensor.

City lookup and weather use the
[Open-Meteo Geocoding API](https://open-meteo.com/en/docs/geocoding-api) and
[Forecast API](https://open-meteo.com/en/docs).

### Local control panel

From a phone or computer on the same network, open:

- `http://maclock.local/`; or
- `http://<device-ip>/`, using the address shown in Diagnostics.

The control panel can change the display, configure alarms and timers, manage
night mode and chimes, select startup sounds, configure MQTT, manage
downloaded MP3s, and install software updates. It is served directly by
Maclock and requires no cloud service.

## 9. MQTT and Home Assistant

Open **MQTT** in the local control panel and enter the broker host or IP,
port (default `1883`), and optional username and password. MQTT is an
unencrypted local connection. Leaving the password blank preserves the stored
password; use the explicit checkbox to clear it.

Maclock publishes retained Home Assistant discovery to:

```text
homeassistant/device/maclock_<chip-id>/config
```

The discovered device includes status plus **Beacon** and **Notification**
notify entities. Their command topics are:

```text
maclock/<chip-id>/beacon/set
maclock/<chip-id>/notification/set
```

Beacon example:

```json
{"id":"doorbell-42","title":"Front door","message":"Someone is outside","timeout":15}
```

Notification example:

```json
{"id":"washer-42","title":"Laundry","message":"The washing is finished"}
```

Beacons disappear and acknowledge after their timeout. Notifications remain
until **OK** is tapped. IDs prevent duplicate delivery. Maclock displays one
message and retains only the newest pending message. An alarm or finished
timer temporarily hides the message and restores it afterward.

## 10. Software Updates and Sounds

Maclock checks for stable releases after Wi-Fi connects and every 24 hours.
Open **System > Software Update** or the Software Update web application to
install, defer, or ignore a release.

| Update settings | Available update |
| --- | --- |
| ![Software Update settings page](img/manual/software-update-page.png) | ![Available update prompt](img/manual/software-update.png) |

Updates do not overwrite or remove sounds. If free space becomes too low, delete
unwanted files deliberately through Sound Manager.

## 11. Mini vMac

Mini vMac emulates a Macintosh Plus in the Maclock display.

| About This Macintosh | MacPaint |
| --- | --- |
| ![About This Macintosh](img/emulator_about.jpg) | ![MacPaint running on Maclock](img/emulator_paint.jpg) |

LittleFS must contain:

- `vMac.ROM`, a compatible 128 KiB Macintosh Plus ROM.
- `disk1.dsk`, the first bootable disk.
- Optional sequential disks named `disk2.dsk` through `disk6.dsk`.

Disk discovery stops at the first missing number. If `disk1.dsk` and
`disk3.dsk` exist without `disk2.dsk`, only `disk1.dsk` mounts.

When Maclock first connects to Wi-Fi, it installs the default ROM and System 7
disk automatically if either file is missing. Existing files are never replaced
by this automatic setup.

### Back up or install ROMs and disks

Prerequisite: exit Mini vMac and open Maclock's web control panel.

1. Open **Mini vMac Files**.
2. Find the ROM or disk slot you want to manage.
3. Select **Download** to save the current file to your computer.
4. To install a file, select **Upload**, then choose a compatible ROM or disk
   image.
5. Wait for **Mini vMac file installed** before closing the page.

> **Warning:** Uploading replaces the selected file. Download any writable disk
> you want to keep before replacing it. The ROM must be a compatible 128 KiB
> Macintosh Plus ROM; disk images must be non-empty and 512-byte aligned.

### Controls

- Drag a finger to move the pointer.
- Use discrete touch as the mouse button.
- Clock acts as Enter.
- Alarm acts as Escape.
- Rotate the encoder to adjust and save brightness.

To leave safely, hold **Clock + Alarm** for two seconds. Maclock ejects and
closes its disk images before returning to Configuration.

### Add Macintosh software

1. Open [System 7.0 at 320×240 in Infinite Mac](https://infinitemac.org/1991/System%207.0?screenSize=320x240).
2. Copy software you are entitled to use to **Saved HD**.
3. Export Saved HD from Infinite Mac.
4. Download a blank image from the
   [Mini vMac blanks page](https://www.gryphel.com/c/minivmac/extras/blanks/).
5. Open both images in [Mini vMac](https://www.gryphel.com/c/minivmac/download.html)
   and copy the software to the blank image.
6. Name the result with the next contiguous disk number.
7. Install it using [BUILD.md](BUILD.md#build-and-upload).

Use only ROMs and software that you are legally entitled to use.

## 12. Touchscreen Calibration

### Calibrate the touchscreen

1. Open Configuration.
2. Release Clock.
3. Press Clock again.
4. Touch and release the four crosshairs in order: top-left, top-right,
   bottom-right, bottom-left.

After the fourth point, calibration is saved and the clock opens. Press Clock
during calibration to cancel without changing the saved values.

![Touchscreen calibration crosshair](img/manual/touch-calibration.png)

## Support

For fault diagnosis and corrective steps, see
[Troubleshooting](TROUBLESHOOTING.md).

For build-specific problems, consult [BUILD.md](BUILD.md). For community help,
join the [Maclock Discord server](https://discord.gg/89etSPMFym).
