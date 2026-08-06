# Maclock User Manual

This manual explains how to use Maclock. For assembly and installation, see
the [Build Your Own guide](BUILD.md).

## 1. Meet Your Maclock

Maclock has a touchscreen, a rotary control, two front buttons, a top touch
sensor, and a floppy lever.

| Control | Clock | Macintosh emulator |
| --- | --- | --- |
| Rotary control | Change brightness, or move through menus when the touchscreen is unavailable | Change brightness |
| Clock button | Open Configuration, dismiss an alarm, or wake the screen | Enter |
| Alarm button | Open Alarms and Timer, snooze an alarm, or wake the screen | Escape |
| Clock + Alarm | Open Configuration | Leave the emulator safely |
| Touchscreen | Use menus and move the pointer | Move the pointer |
| Top touch sensor | Temporarily use full brightness or snooze an alarm | Press the mouse button |

### Using Maclock without a touchscreen

Maclock can still operate as a clock when its touchscreen is unavailable.

- Turn the rotary control to move the blinking selection through a menu.
- Touch the top sensor to activate the selected item.
- On the clock and screensavers, the rotary control changes brightness.
- The Clock and Alarm buttons keep their usual shortcuts.
- Calibration and the Macintosh emulator remain unavailable until the
  touchscreen is reconnected and Maclock is restarted.

![Rotary focus shown around the selected General button](img/manual/touchless-focus.png)

## 2. Getting Started

1. Connect Maclock to power.
2. Insert the floppy when the alternating floppy symbols appear.
3. Wait while Maclock starts.
4. The selected clock face appears.

| Configuration | Macintosh clock |
| --- | --- |
| ![Configuration section hub](img/manual/configuration-hub.png) | ![Normal Macintosh clock face](img/manual/clock-macintosh.png) |

If Maclock shows a warning during startup, restart it once. If the warning
returns, see [Troubleshooting](TROUBLESHOOTING.md).

## 3. Configuration

Press **Clock** from the normal clock to open Configuration. You can also hold
Clock while connecting power.

Configuration is divided into four sections:

- **General** — language, regional choices, date, and time.
- **Display** — clock faces, appearance, screensavers, and night mode.
- **Sound** — hourly chimes, sounds, volume, and quiet hours.
- **System** — startup choice, Wi-Fi, updates, tools, and About.

Use **Sections** to return to the section chooser. Use **Exit** to return to
the clock.

### Choose what starts

1. Open **System**, then **Start**.
2. Select the large **Clock** or **Emulator** button to open it now.
3. Select **Boot: Clock** or **Boot: Emulator** to choose what Maclock opens
   automatically next time.

| Brightness at startup | Startup choice |
| --- | --- |
| ![Brightness startup preferences](img/manual/preferences.png) | ![Clock or emulator startup selection](img/manual/start-mode.png) |

You can also choose whether Maclock starts at the previous brightness, its
lowest visible brightness, or full brightness.

## 4. Language, Region, Date, and Time

### Language

Open **General > Language**, then choose English, Français, Español, Deutsch,
or Italiano. Maclock remembers your choice.

![Language selection](img/manual/language.png)

### Regional choices

Open **General > Regional** to choose:

- The order of day, month, and year.
- 12-hour or 24-hour time.
- Celsius or Fahrenheit.

![Regional date, time, and temperature choices](img/manual/regional.png)

### Set the date and time

1. Open **General > Date / Time**.
2. Select the part you want to change.
3. Use **-** and **+**. Hold either button to repeat.

![Date and time editor](img/manual/date-time.png)

## 5. Clock Faces and Display

### Display options

Open **Display > Display** to choose whether to show:

- An initial zero before single-digit hours.
- The weekday.
- Seconds.
- A light or dark appearance.

![Basic display options](img/manual/display.png)

### Clock faces

Open **Display > Clock Face** and choose the face you prefer.

| Macintosh | Compact |
| --- | --- |
| ![Macintosh display mode](img/manual/clock-macintosh.png) | ![Compact display mode](img/manual/clock-compact.png) |

| Analog | Flip |
| --- | --- |
| ![Analog display mode](img/manual/clock-analog.png) | ![Flip display mode](img/manual/clock-flip.png) |

| Odometer | Mac OS 8 |
| --- | --- |
| ![Odometer display mode](img/manual/clock-odometer.png) | ![Mac OS 8 display mode](img/manual/clock-macos8.png) |

You can also choose an accent color, numeral size, weather display, and Flip
animation speed.

| Clock face | Style | Details |
| --- | --- | --- |
| ![Clock face choices](img/manual/clock-face.png) | ![Accent and numeral size](img/manual/face-style.png) | ![Weather and animation detail](img/manual/face-details.png) |

## 6. Screensavers

Open **Display > Screensaver** to choose what appears after Maclock has not
been used for a while.

![Screensaver settings](img/manual/screensaver.png)

| After Dark | Starfield |
| --- | --- |
| ![After Dark screensaver](img/manual/screensaver-after-dark.png) | ![Starfield screensaver](img/manual/screensaver-starfield.png) |

| Bouncing Mac | Matrix Rain |
| --- | --- |
| ![Bouncing Mac screensaver](img/manual/screensaver-bouncing-mac.png) | ![Matrix Rain screensaver](img/manual/screensaver-matrix-rain.png) |

| Pipes | Flying Clocks |
| --- | --- |
| ![Pipes screensaver](img/manual/screensaver-pipes.png) | ![Flying Clocks screensaver](img/manual/screensaver-flying-clocks.png) |

| Flying Toasters | Marquee |
| --- | --- |
| ![Flying Toasters screensaver](img/manual/screensaver-flying-toasters.png) | ![Marquee screensaver](img/manual/screensaver-marquee.png) |

| Digital Rain Clock | Mystify |
| --- | --- |
| ![Digital Rain Clock screensaver](img/manual/screensaver-digital-rain-clock.png) | ![Mystify screensaver](img/manual/screensaver-mystify.png) |

| Aquarium | Life |
| --- | --- |
| ![Aquarium screensaver](img/manual/screensaver-aquarium.png) | ![Life screensaver](img/manual/screensaver-life.png) |

| Maze | Error Parade |
| --- | --- |
| ![Maze screensaver](img/manual/screensaver-maze.png) | ![Error Parade screensaver](img/manual/screensaver-error-parade.png) |

| Rainy Window | Fireworks |
| --- | --- |
| ![Rainy Window screensaver](img/manual/screensaver-rainy-window.png) | ![Fireworks screensaver](img/manual/screensaver-fireworks.png) |

| Photo Slideshow |
| --- |

- Select **Play** beside a choice to preview it without changing your saved
  screensaver.
- Select **Random** to rotate between available screensavers.
- Select **Off** to disable automatic screensavers.
- Use the Screensaver application in the web control panel to add, view, and
  remove slideshow photographs.

## 7. Night Mode

Night mode can lower the brightness automatically and optionally turn off the
screen while keeping the clock and alarms active.

1. Open **Display > Night**.
2. Enable the schedule and choose its beginning and end.
3. Choose whether to dim the display or turn it off.

| Night schedule | Night behavior |
| --- | --- |
| ![Night mode schedule](img/manual/night-schedule.png) | ![Night mode behavior](img/manual/night-behavior.png) |

Touch the screen or top sensor, or press either front button, to wake the
display temporarily. Alarms and completed timers wake it automatically.

## 8. Alarms

### Set an alarm

1. Press **Alarm** on the clock.
2. Select **Alarm**, then choose one of the three alarms.
3. Set the hour and minute.
4. Choose a repeating schedule or a one-time alarm.
5. Optionally add a label.
6. Choose a sound and volume.
7. Optionally enable gradual volume and the sunrise display.
8. Select **Save**.

| Alarm or Timer | Select alarm | Set time |
| --- | --- | --- |
| ![Alarm and timer chooser](img/manual/alarm-timer-home.png) | ![Alarm selection](img/manual/alarm-selection.png) | ![Alarm time controls](img/manual/alarm-time.png) |

| Repeat days | Sound | Volume |
| --- | --- | --- |
| ![Weekly alarm days](img/manual/alarm-days.png) | ![Alarm sound selection](img/manual/alarm-sound.png) | ![Alarm volume](img/manual/alarm-volume.png) |

![Alarm save and cancel actions](img/manual/alarm-actions.png)

The alarm page shows the next scheduled alarm. A one-time alarm turns itself
off after it has rung.

When an alarm rings:

- Press **Alarm**, touch the top sensor, or select **Snooze 9 min** to snooze.
- Press **Clock** or select **Dismiss** to stop it.

## 9. Timer

1. Press **Alarm** on the clock.
2. Select **Timer**.
3. Adjust the duration with **-10**, **-1**, **+1**, and **+10**.
4. Select **Start**.

![Timer duration editor](img/manual/timer-editor.png)

The timer keeps running while the clock is displayed. Its remaining time
replaces the date. When it finishes, select **Dismiss** or press Clock or
Alarm.

## 10. Hourly Chime

Open the pages under **Sound** to choose:

- No chime, an hourly chime, or a quarter-hour chime.
- The chime sound.
- The volume.
- Quiet hours during which no chime plays.

| Schedule | Sound | Volume | Quiet hours |
| --- | --- | --- | --- |
| ![Chime schedule](img/manual/chime-mode.png) | ![Chime sound](img/manual/chime-sound.png) | ![Chime volume](img/manual/chime-volume.png) | ![Chime quiet hours](img/manual/quiet-hours.png) |

## 11. Wi-Fi and Weather

Wi-Fi is optional. The clock, alarms, timer, and local temperature display
continue to work without it.

### Connect to Wi-Fi

1. Open **System > Wi-Fi**.
2. Select **Setup Wi-Fi**.
3. Scan the QR code with your phone and follow the displayed instructions.
4. Select your network and enter its password.
5. Enter your city and country for local time and weather.
6. Save, then return to Maclock.

| Wi-Fi settings | Setup QR code |
| --- | --- |
| ![Wi-Fi settings](img/manual/wifi.png) | ![Wi-Fi setup QR code](img/manual/wifi-setup-qr.png) |

Once connected, Maclock can set its time automatically, display online
weather, check for updates, and provide its web control panel.

## 12. Web Control Panel

Use a phone or computer connected to the same network as Maclock. Open the
address shown by Maclock in **System > Tools > Diagnostics**.

On a computer, click an icon once to select it and double-click to open it. On
a touchscreen, tap once. Close the window or click outside it to return to the
launcher.

![Macintosh-style web control panel launcher](img/manual/control-panel-launcher.png)

The control panel lets you manage appearance, location, screensavers, timers,
alarms, night mode, chimes, sounds, updates, backups, and emulator files.

If you close a window containing unsaved changes, choose whether to continue
editing or discard those changes.

### Add and manage sounds

Open **Sound Manager** to:

- Drag an MP3 into the window or choose one from your device.
- Import an MP3 from a web address.
- Search MyInstants.
- Preview sounds at different volumes.
- Remove sounds you previously added.

Built-in sounds remain available but cannot be removed.

![Sound Manager in the web control panel](img/manual/sound-manager-web.png)

### Back up your settings

Open **Configuration Backup** to save a copy of your settings. Keep a recent
backup, especially before making many changes. The same application restores a
previous backup.

### Optional home automation

The **MQTT** application connects Maclock to compatible home-automation
systems. Enter the connection details supplied by your home-automation setup.
Maclock then appears as a device that can display notifications.

## 13. Software Updates

Maclock checks for updates when it is online. You can also open
**System > Software Update** or **Software Update** in the web control panel.

| Update settings | Available update |
| --- | --- |
| ![Software Update settings page](img/manual/software-update-page.png) | ![Available update prompt](img/manual/software-update.png) |

- Select **Update** to install the new version.
- Select **Later** to postpone it.
- Select **Ignore** to stop being reminded about that version.

Keep Maclock connected to power and Wi-Fi until the update has finished. A
progress bar shows the installation status. If an update is interrupted,
restart Maclock and leave it connected so it can continue.

Sounds and emulator files that you added are kept during normal updates. If
Maclock reports that it needs more space, remove unwanted sounds in Sound
Manager and try again.

## 14. Tools and About

Open **System > Tools** to view Diagnostics and maintenance options.
Diagnostics shows whether Maclock's main features are available and provides
useful information when asking for help.

| Tools | Diagnostics | About |
| --- | --- | --- |
| ![System tools](img/manual/tools.png) | ![Maclock diagnostics](img/manual/diagnostics.png) | ![Maclock author and project information](img/manual/about.png) |

The **About** page shows the author and a QR code linking to the Maclock
project.

## 15. Macintosh Emulator

The emulator recreates a classic Macintosh on Maclock. It is available only
when the touchscreen is connected.

| About This Macintosh | MacPaint |
| --- | --- |
| ![About This Macintosh](img/emulator_about.jpg) | ![MacPaint running on Maclock](img/emulator_paint.jpg) |

### Controls

- Drag a finger to move the pointer.
- Touch the top sensor to click.
- Clock acts as Enter.
- Alarm acts as Escape.
- Turn the rotary control to change brightness.

To leave safely, hold **Clock + Alarm** for two seconds. Always leave the
emulator this way before disconnecting power so your Macintosh files are saved
correctly.

### Manage emulator files

Open **Mini vMac Files** in the web control panel to back up, replace, or add
emulator files. Back up anything important before replacing it, then wait for
Maclock to confirm that the new file is installed.

Use only software that you are legally entitled to use.

## 16. Touchscreen Calibration

Calibrate the touchscreen if taps no longer match the displayed controls.

1. Open Configuration.
2. Press Clock again.
3. Touch and release each crosshair in order.

After the fourth crosshair, Maclock saves the calibration and returns to the
clock. Press Clock during calibration to cancel without saving.

![Touchscreen calibration crosshair](img/manual/touch-calibration.png)