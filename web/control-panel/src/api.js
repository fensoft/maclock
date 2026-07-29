const clone = (value) => JSON.parse(JSON.stringify(value));

const demoState = {
  appearance: {
    language: 0,
    face: 0,
    theme: 0,
    accent: 0,
    fontSize: 1,
    weather: true,
    flipSpeed: 1,
    brightness: 7,
    hourFormat: 0,
    leadingZero: true,
    seconds: true,
    weekday: false,
  },
  location: {
    city: "Paris",
    country: "FR",
    resolved: "Paris, FR",
    timezone: "Europe/Paris",
  },
  mqtt: {
    enabled: false,
    host: "",
    port: 1883,
    username: "",
    passwordSet: false,
    connected: false,
    status: "Disabled",
    deviceId: "maclock_simulator",
    topicBase: "maclock/simulator",
    displayState: "idle",
    currentId: "",
    pendingId: "",
    lastId: "",
    lastResult: "",
    lastError: "",
  },
  screensaver: {
    mode: 1,
    delay: 1,
    active: false,
  },
  systemSounds: {
    startup: "/startup.mp3",
    startupVolume: 80,
    floppy: "/floppy.mp3",
    floppyVolume: 60,
  },
  night: {
    enabled: false,
    start: 22,
    end: 7,
    screenOff: false,
    offHour: 0,
  },
  chime: {
    mode: 0,
    sound: "/quack.mp3",
    volume: 2,
    quiet: true,
    quietStart: 22,
    quietEnd: 8,
  },
  alarms: [
    {
      enabled: true,
      hour: 7,
      minute: 30,
      weekdays: 31,
      sound: "/quack.mp3",
      volume: 4,
      oneTime: false,
      gradualVolume: true,
      sunrise: true,
      label: "Wake up",
    },
    {
      enabled: false,
      hour: 9,
      minute: 0,
      weekdays: 96,
      sound: "/alarm.mp3",
      volume: 4,
      oneTime: false,
      gradualVolume: false,
      sunrise: false,
      label: "Weekend",
    },
    {
      enabled: false,
      hour: 12,
      minute: 0,
      weekdays: 127,
      sound: "/chime.mp3",
      volume: 2,
      oneTime: true,
      gradualVolume: false,
      sunrise: false,
      label: "",
    },
  ],
  upcomingAlarm: {
    valid: true,
    snoozed: false,
    oneTime: false,
    index: 0,
    dayOffset: 1,
    weekday: 1,
    hour: 7,
    minute: 30,
    label: "Wake up",
  },
  timer: {
    active: false,
    minutes: 25,
    remaining: 0,
    sound: "/quack.mp3",
    volume: 4,
  },
  update: {
    stage: "upToDate",
    supported: true,
    busy: false,
    available: false,
    prompt: false,
    rebootRequired: false,
    progress: 0,
    changedAssets: 0,
    currentVersion: "1.0.0",
    assetVersion: "1.0.0",
    latestVersion: "1.0.0",
    releaseUrl: "https://github.com/fensoft/maclock/releases/tag/v1.0.0",
    releaseNotes: "First stable Maclock release.",
    message: "Maclock is up to date",
  },
  sounds: [
    { path: "/alarm.mp3", name: "Alarm", size: 32480, downloaded: false },
    { path: "/chime.mp3", name: "Chime", size: 18432, downloaded: false },
    { path: "/floppy.mp3", name: "Floppy", size: 12288, builtIn: true, downloaded: false },
    { path: "/quack.mp3", name: "Quack", size: 24576, builtIn: true, downloaded: false },
    { path: "/startup.mp3", name: "Startup", size: 48128, builtIn: true, downloaded: false },
  ],
  storage: {
    total: 10420224,
    used: 4521984,
    free: 5898240,
  },
};

let demoTimerEndsAt = 0;

function bodyFor(values) {
  const body = new URLSearchParams();
  Object.entries(values).forEach(([key, value]) => {
    body.set(key, String(value));
  });
  return body;
}

function refreshDemoSoundUsage() {
  const used = new Set([
    demoState.systemSounds.startup,
    demoState.systemSounds.floppy,
    demoState.chime.sound,
    demoState.timer.sound,
    ...demoState.alarms.map((alarm) => alarm.sound),
  ]);
  demoState.sounds.forEach((sound) => {
    sound.inUse = used.has(sound.path);
  });
}

function refreshDemoUpcomingAlarm() {
  const now = new Date();
  let best = null;
  demoState.alarms.forEach((alarm, index) => {
    if (!alarm.enabled) return;
    for (let dayOffset = 0; dayOffset <= 7; dayOffset += 1) {
      const candidate = new Date(now);
      candidate.setDate(now.getDate() + dayOffset);
      candidate.setHours(alarm.hour, alarm.minute, 0, 0);
      if (candidate <= now) continue;
      const mondayWeekday = (candidate.getDay() + 6) % 7;
      if (
        !alarm.oneTime &&
        (alarm.weekdays & (1 << mondayWeekday)) === 0
      ) {
        continue;
      }
      if (!best || candidate < best.candidate) {
        best = { alarm, index, dayOffset, mondayWeekday, candidate };
      }
      break;
    }
  });
  demoState.upcomingAlarm = best
    ? {
        valid: true,
        snoozed: false,
        oneTime: best.alarm.oneTime,
        index: best.index,
        dayOffset: best.dayOffset,
        weekday: best.mondayWeekday,
        hour: best.alarm.hour,
        minute: best.alarm.minute,
        label: best.alarm.label,
      }
    : { valid: false };
}

function uniqueDemoSound(name, size = 0) {
  const cleanName = String(name || "sound.mp3")
    .replace(/^.*[\\/]/, "")
    .replace(/\.mp3$/i, "")
    .replace(/[^a-z0-9 _-]/gi, "_")
    .trim() || "sound";
  let suffix = "";
  let number = 2;
  let path = `/downloaded/${cleanName}.mp3`;
  while (demoState.sounds.some((sound) => sound.path === path)) {
    suffix = `-${number++}`;
    path = `/downloaded/${cleanName}${suffix}.mp3`;
  }
  const entry = {
    path,
    name: `${cleanName}${suffix}`,
    size,
    builtIn: false,
    downloaded: true,
    inUse: false,
  };
  demoState.sounds.push(entry);
  demoState.sounds.sort((a, b) => a.name.localeCompare(b.name));
  return entry;
}

async function parseResponse(response) {
  let payload = {};
  try {
    payload = await response.json();
  } catch {
    payload = {};
  }
  if (!response.ok || payload.ok === false) {
    throw new Error(payload.message || `Request failed (${response.status})`);
  }
  return payload;
}

async function realFetch(path) {
  const response = await fetch(path, { cache: "no-store" });
  return parseResponse(response);
}

function applyDemo(path, values) {
  const number = (key) => Number(values[key]);
  if (path === "/api/appearance") {
    const appearance = {
      language: number("language"),
      face: number("face"),
      theme: number("theme"),
      brightness: number("brightness"),
      hourFormat: number("hourFormat"),
      leadingZero: number("leadingZero") !== 0,
      seconds: number("seconds") !== 0,
      weekday: number("weekday") !== 0,
    };
    if ("accent" in values) appearance.accent = number("accent");
    if ("fontSize" in values) appearance.fontSize = number("fontSize");
    if ("weather" in values) {
      appearance.weather = number("weather") !== 0;
    }
    if ("flipSpeed" in values) {
      appearance.flipSpeed = number("flipSpeed");
    }
    Object.assign(demoState.appearance, appearance);
  } else if (path === "/api/location") {
    Object.assign(demoState.location, {
      city: String(values.city || "").trim(),
      country: String(values.country || "").trim().toUpperCase(),
      resolved: [
        String(values.city || "").trim(),
        String(values.country || "").trim().toUpperCase(),
      ]
        .filter(Boolean)
        .join(", "),
      timezone: "Updating…",
    });
  } else if (path === "/api/mqtt") {
    Object.assign(demoState.mqtt, {
      enabled: number("enabled") !== 0,
      host: String(values.host || "").trim(),
      port: number("port"),
      username: String(values.username || "").trim(),
    });
    if (number("clearPassword") !== 0) {
      demoState.mqtt.passwordSet = false;
    } else if (values.password) {
      demoState.mqtt.passwordSet = true;
    }
    demoState.mqtt.connected =
      demoState.mqtt.enabled && Boolean(demoState.mqtt.host);
    demoState.mqtt.status = demoState.mqtt.connected
      ? "Connected (demo)"
      : demoState.mqtt.enabled
        ? "Disconnected"
        : "Disabled";
  } else if (path === "/api/screensaver") {
    Object.assign(demoState.screensaver, {
      mode: number("mode"),
      delay: number("delay"),
      active: values.action === "launch",
    });
  } else if (path === "/api/alarm") {
    const alarm = demoState.alarms[number("index")];
    Object.assign(alarm, {
      enabled: number("enabled") !== 0,
      hour: number("hour"),
      minute: number("minute"),
      weekdays: number("weekdays"),
      sound: values.sound,
      volume: number("volume"),
      oneTime: number("oneTime") !== 0,
      gradualVolume: number("gradualVolume") !== 0,
      sunrise: number("sunrise") !== 0,
      label: String(values.label || "").trim(),
    });
    refreshDemoUpcomingAlarm();
  } else if (path === "/api/timer") {
    Object.assign(demoState.timer, {
      minutes: number("minutes"),
      sound: values.sound,
      volume: number("volume"),
    });
    if (values.action === "start") {
      demoState.timer.active = true;
      demoTimerEndsAt = Date.now() + demoState.timer.minutes * 60000;
    } else if (values.action === "cancel") {
      demoState.timer.active = false;
      demoState.timer.remaining = 0;
      demoTimerEndsAt = 0;
    }
  } else if (path === "/api/night") {
    Object.assign(demoState.night, {
      enabled: number("enabled") !== 0,
      start: number("start"),
      end: number("end"),
      screenOff: number("screenOff") !== 0,
      offHour: number("offHour"),
    });
  } else if (path === "/api/chime") {
    Object.assign(demoState.chime, {
      mode: number("mode"),
      sound: values.sound,
      volume: number("volume"),
      quiet: number("quiet") !== 0,
      quietStart: number("quietStart"),
      quietEnd: number("quietEnd"),
    });
  } else if (path === "/api/sounds") {
    Object.assign(demoState.systemSounds, {
      startup: values.startup,
      startupVolume: number("startupVolume"),
      floppy: values.floppy,
      floppyVolume: number("floppyVolume"),
    });
  } else if (path === "/api/update/check") {
    demoState.update.stage = "available";
    demoState.update.available = true;
    demoState.update.latestVersion = "1.1.0";
    demoState.update.message = "A new Maclock release is available";
  } else if (path === "/api/update/install") {
    demoState.update.stage = "readyToReboot";
    demoState.update.available = false;
    demoState.update.rebootRequired = true;
    demoState.update.progress = 100;
    demoState.update.changedAssets = 4;
    demoState.update.message = "Update installed; reboot to finish";
  } else if (path === "/api/update/dismiss") {
    demoState.update.prompt = false;
  }
}

export async function fetchState() {
  if (!import.meta.env.DEV) return realFetch("/api/state");
  await new Promise((resolve) => setTimeout(resolve, 180));
  refreshDemoSoundUsage();
  refreshDemoUpcomingAlarm();
  return clone(demoState);
}

export async function fetchStatus() {
  if (!import.meta.env.DEV) return realFetch("/api/status");
  if (demoState.timer.active) {
    demoState.timer.remaining = Math.max(
      0,
      Math.ceil((demoTimerEndsAt - Date.now()) / 1000),
    );
    if (!demoState.timer.remaining) demoState.timer.active = false;
  }
  return {
    timer: {
      active: demoState.timer.active,
      remaining: demoState.timer.remaining,
    },
    screensaver: {
      active: demoState.screensaver.active,
    },
    upcomingAlarm: clone(demoState.upcomingAlarm),
    update: clone(demoState.update),
    mqtt: clone(demoState.mqtt),
  };
}

export async function postForm(path, values) {
  if (import.meta.env.DEV) {
    await new Promise((resolve) => setTimeout(resolve, 120));
    applyDemo(path, values);
    return { ok: true, message: "Saved on demo Maclock" };
  }

  const response = await fetch(path, {
    method: "POST",
    headers: { "Content-Type": "application/x-www-form-urlencoded" },
    body: bodyFor(values),
  });
  return parseResponse(response);
}

export async function uploadSound(file) {
  if (import.meta.env.DEV) {
    await new Promise((resolve) => setTimeout(resolve, 350));
    const entry = uniqueDemoSound(file?.name, file?.size || 0);
    return { ok: true, message: "Sound uploaded", path: entry.path };
  }

  const body = new FormData();
  body.append("file", file, file.name);
  const response = await fetch("/api/sound/upload", {
    method: "POST",
    body,
  });
  return parseResponse(response);
}

export async function importSoundUrl(url, suggestedName = "") {
  if (import.meta.env.DEV) {
    await new Promise((resolve) => setTimeout(resolve, 450));
    let name = "imported-sound.mp3";
    if (suggestedName.trim()) name = suggestedName.trim();
    try {
      if (!suggestedName.trim()) {
        const parsed = new URL(url);
        name = decodeURIComponent(parsed.pathname.split("/").pop()) || name;
        if (!/\.mp3$/i.test(name)) name = "myinstants-sound.mp3";
      }
    } catch {
      // The firmware performs the authoritative URL validation.
    }
    const entry = uniqueDemoSound(name, 65536);
    return { ok: true, message: "Sound imported", path: entry.path };
  }
  return postForm("/api/sound/import", { url, name: suggestedName });
}

export async function searchMyInstants(query) {
  if (import.meta.env.DEV) {
    await new Promise((resolve) => setTimeout(resolve, 500));
    const normalized = query.trim().toLowerCase();
    const samples = [
      {
        name: "DJ Airhorn",
        mp3Url:
          "https://www.myinstants.com/media/sounds/dj-airhorn-sound-effect-kingbeatz_1.mp3",
        pageUrl: "https://www.myinstants.com/en/instant/dj-airhorn/",
      },
      {
        name: "VERY LOUD AIRHORN",
        mp3Url:
          "https://www.myinstants.com/media/sounds/veryloudairhorn.mp3",
        pageUrl:
          "https://www.myinstants.com/en/instant/very-loud-airhorn/",
      },
      {
        name: "Macintosh Startup",
        mp3Url:
          "https://www.myinstants.com/media/sounds/mac-startup.mp3",
        pageUrl:
          "https://www.myinstants.com/en/instant/macintosh-startup/",
      },
    ];
    return {
      ok: true,
      query,
      results: samples.filter(
        (result) =>
          !normalized || result.name.toLowerCase().includes(normalized),
      ),
    };
  }
  return postForm("/api/sound/myinstants/search", { query });
}

export async function deleteSound(path) {
  if (import.meta.env.DEV) {
    await new Promise((resolve) => setTimeout(resolve, 250));
    refreshDemoSoundUsage();
    const index = demoState.sounds.findIndex((sound) => sound.path === path);
    if (index < 0) throw new Error("Sound file was not found");
    if (!demoState.sounds[index].downloaded) {
      throw new Error("Only downloaded sounds can be removed");
    }
    if (demoState.sounds[index].inUse) {
      throw new Error("This sound is currently in use");
    }
    demoState.sounds.splice(index, 1);
    return { ok: true, message: "Sound removed" };
  }
  return postForm("/api/sound/delete", { sound: path });
}

export async function uploadFirmware(file) {
  if (import.meta.env.DEV) {
    await new Promise((resolve) => setTimeout(resolve, 600));
    demoState.update.stage = "readyToReboot";
    demoState.update.rebootRequired = true;
    demoState.update.progress = 100;
    demoState.update.message = "Firmware uploaded; reboot to finish";
    return { ok: true, message: demoState.update.message };
  }
  const body = new FormData();
  body.append("file", file, file.name);
  const response = await fetch("/api/update/firmware", {
    method: "POST",
    body,
  });
  return parseResponse(response);
}

export async function exportConfiguration() {
  if (import.meta.env.DEV) {
    await new Promise((resolve) => setTimeout(resolve, 350));
    return new Blob(
      [
        JSON.stringify(
          {
            format: "maclock-configuration-demo",
            note: "The firmware produces a streamed ZIP archive.",
          },
          null,
          2,
        ),
      ],
      { type: "application/zip" },
    );
  }
  return null;
}

export async function restoreConfiguration(file) {
  if (import.meta.env.DEV) {
    await new Promise((resolve) => setTimeout(resolve, 650));
    return {
      ok: true,
      message: "Demo backup restored",
      warnings: [],
      networkChanged: false,
    };
  }
  const body = new FormData();
  body.append("file", file, file.name);
  const response = await fetch("/api/configuration/import", {
    method: "POST",
    body,
  });
  return parseResponse(response);
}
