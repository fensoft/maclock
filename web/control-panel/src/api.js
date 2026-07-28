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
    { path: "/alarm.mp3", name: "Alarm" },
    { path: "/chime.mp3", name: "Chime" },
    { path: "/floppy.mp3", name: "Floppy" },
    { path: "/quack.mp3", name: "Quack" },
    { path: "/startup.mp3", name: "Startup" },
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
