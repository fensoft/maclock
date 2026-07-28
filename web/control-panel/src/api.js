const clone = (value) => JSON.parse(JSON.stringify(value));

const demoState = {
  appearance: {
    language: 0,
    face: 0,
    theme: 0,
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
    },
    {
      enabled: false,
      hour: 9,
      minute: 0,
      weekdays: 96,
      sound: "/alarm.mp3",
      volume: 4,
    },
    {
      enabled: false,
      hour: 12,
      minute: 0,
      weekdays: 127,
      sound: "/chime.mp3",
      volume: 2,
    },
  ],
  timer: {
    active: false,
    minutes: 25,
    remaining: 0,
    sound: "/quack.mp3",
    volume: 4,
  },
  sounds: [
    { path: "/alarm.mp3", name: "Alarm", size: 32480 },
    { path: "/chime.mp3", name: "Chime", size: 18432 },
    { path: "/floppy.mp3", name: "Floppy", size: 12288, builtIn: true },
    { path: "/quack.mp3", name: "Quack", size: 24576, builtIn: true },
    { path: "/startup.mp3", name: "Startup", size: 48128, builtIn: true },
  ],
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

function uniqueDemoSound(name, size = 0) {
  const cleanName = String(name || "sound.mp3")
    .replace(/^.*[\\/]/, "")
    .replace(/\.mp3$/i, "")
    .replace(/[^a-z0-9 _-]/gi, "_")
    .trim() || "sound";
  let suffix = "";
  let number = 2;
  let path = `/${cleanName}.mp3`;
  while (demoState.sounds.some((sound) => sound.path === path)) {
    suffix = `-${number++}`;
    path = `/${cleanName}${suffix}.mp3`;
  }
  const entry = {
    path,
    name: `${cleanName}${suffix}`,
    size,
    builtIn: false,
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
    Object.assign(demoState.appearance, {
      language: number("language"),
      face: number("face"),
      theme: number("theme"),
      brightness: number("brightness"),
      hourFormat: number("hourFormat"),
      leadingZero: number("leadingZero") !== 0,
      seconds: number("seconds") !== 0,
      weekday: number("weekday") !== 0,
    });
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
    });
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
  }
}

export async function fetchState() {
  if (!import.meta.env.DEV) return realFetch("/api/state");
  await new Promise((resolve) => setTimeout(resolve, 180));
  refreshDemoSoundUsage();
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

export async function importSoundUrl(url) {
  if (import.meta.env.DEV) {
    await new Promise((resolve) => setTimeout(resolve, 450));
    let name = "imported-sound.mp3";
    try {
      const parsed = new URL(url);
      name = decodeURIComponent(parsed.pathname.split("/").pop()) || name;
      if (!/\.mp3$/i.test(name)) name = "myinstants-sound.mp3";
    } catch {
      // The firmware performs the authoritative URL validation.
    }
    const entry = uniqueDemoSound(name, 65536);
    return { ok: true, message: "Sound imported", path: entry.path };
  }
  return postForm("/api/sound/import", { url });
}

export async function deleteSound(path) {
  if (import.meta.env.DEV) {
    await new Promise((resolve) => setTimeout(resolve, 250));
    refreshDemoSoundUsage();
    const index = demoState.sounds.findIndex((sound) => sound.path === path);
    if (index < 0) throw new Error("Sound file was not found");
    if (demoState.sounds[index].builtIn) {
      throw new Error("Built-in sounds cannot be removed");
    }
    if (demoState.sounds[index].inUse) {
      throw new Error("This sound is currently in use");
    }
    demoState.sounds.splice(index, 1);
    return { ok: true, message: "Sound removed" };
  }
  return postForm("/api/sound/delete", { sound: path });
}
