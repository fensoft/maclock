<script setup>
import { computed, onBeforeUnmount, onMounted, ref } from "vue";

import { fetchState, fetchStatus, postForm } from "./api";
import MacButton from "./components/MacButton.vue";
import MacWindow from "./components/MacWindow.vue";

const faceOptions = ["Macintosh", "Compact Digital", "Analog", "Flip Clock"];
const themeOptions = ["Light", "Dark"];
const chimeOptions = ["Off", "Hourly", "Quarter-hour"];
const weekdays = ["M", "T", "W", "T", "F", "S", "S"];
const volumeOptions = ["Mute", "Low", "Medium", "High"];

const panelState = ref(null);
const loading = ref(true);
const busy = ref("");
const notice = ref(null);
const timerRemaining = ref(0);
let statusPoll = 0;
let noticeTimeout = 0;

const sounds = computed(() => panelState.value?.sounds || []);
const timerText = computed(() => {
  const seconds = Math.max(0, timerRemaining.value);
  const hours = Math.floor(seconds / 3600);
  const minutes = Math.floor((seconds % 3600) / 60);
  const rest = seconds % 60;
  return [hours, minutes, rest]
    .map((value) => String(value).padStart(2, "0"))
    .join(":");
});

function showNotice(message, kind = "success") {
  notice.value = { message, kind };
  window.clearTimeout(noticeTimeout);
  noticeTimeout = window.setTimeout(() => {
    notice.value = null;
  }, 3800);
}

async function loadState({ quiet = false } = {}) {
  if (!quiet) loading.value = true;
  try {
    panelState.value = await fetchState();
    timerRemaining.value = panelState.value.timer.remaining || 0;
  } catch (error) {
    showNotice(error.message || "Could not contact Maclock", "error");
  } finally {
    loading.value = false;
  }
}

async function runAction(
  key,
  path,
  values,
  successMessage,
  { refresh = true } = {},
) {
  if (busy.value) return;
  busy.value = key;
  try {
    const result = await postForm(path, values);
    showNotice(result.message || successMessage);
    if (refresh) await loadState({ quiet: true });
  } catch (error) {
    showNotice(error.message || "The setting was not saved", "error");
  } finally {
    busy.value = "";
  }
}

function saveAppearance() {
  const appearance = panelState.value.appearance;
  runAction("appearance", "/api/appearance", appearance, "Appearance saved");
}

function alarmWeekdayEnabled(alarm, index) {
  return (alarm.weekdays & (1 << index)) !== 0;
}

function toggleAlarmWeekday(alarm, index) {
  alarm.weekdays ^= 1 << index;
}

function saveAlarm(index) {
  const alarm = panelState.value.alarms[index];
  runAction(
    `alarm-${index}`,
    "/api/alarm",
    {
      index,
      enabled: alarm.enabled ? 1 : 0,
      hour: alarm.hour,
      minute: alarm.minute,
      weekdays: alarm.weekdays,
      sound: alarm.sound,
      volume: alarm.volume,
    },
    `Alarm ${index + 1} saved`,
  );
}

function timerAction(action) {
  const timer = panelState.value.timer;
  runAction(
    `timer-${action}`,
    "/api/timer",
    {
      action,
      minutes: timer.minutes,
      sound: timer.sound,
      volume: timer.volume,
    },
    action === "start" ? "Timer started" : "Timer saved",
  );
}

function saveNightMode() {
  const night = panelState.value.night;
  runAction(
    "night",
    "/api/night",
    {
      enabled: night.enabled ? 1 : 0,
      start: night.start,
      end: night.end,
      screenOff: night.screenOff ? 1 : 0,
      offHour: night.offHour,
    },
    "Night mode saved",
  );
}

function saveChime() {
  const chime = panelState.value.chime;
  runAction(
    "chime",
    "/api/chime",
    {
      mode: chime.mode,
      sound: chime.sound,
      volume: chime.volume,
      quiet: chime.quiet ? 1 : 0,
      quietStart: chime.quietStart,
      quietEnd: chime.quietEnd,
    },
    "Chime saved",
  );
}

function saveSystemSounds() {
  const system = panelState.value.systemSounds;
  runAction(
    "sounds",
    "/api/sounds",
    system,
    "System sounds saved",
  );
}

function previewSound(sound, volume, levelScale = false) {
  const previewVolume = levelScale
    ? Math.min(100, Math.max(1, Number(volume) * 25 + 25))
    : Math.min(100, Math.max(1, Number(volume)));
  runAction(
    `preview-${sound}`,
    "/api/preview",
    { sound, volume: previewVolume },
    "Playing sound",
    { refresh: false },
  );
}

async function pollStatus() {
  if (!panelState.value) return;
  try {
    const status = await fetchStatus();
    panelState.value.timer.active = status.timer.active;
    panelState.value.timer.remaining = status.timer.remaining;
    timerRemaining.value = status.timer.remaining || 0;
  } catch {
    // Keep the last known timer state during a transient Wi-Fi interruption.
  }
}

onMounted(async () => {
  await loadState();
  statusPoll = window.setInterval(pollStatus, 3000);
});

onBeforeUnmount(() => {
  window.clearInterval(statusPoll);
  window.clearTimeout(noticeTimeout);
});
</script>

<template>
  <div class="desktop">
    <nav class="menu-bar" aria-label="Control panel sections">
      <a class="apple" href="#welcome" aria-label="Maclock home">◆</a>
      <a href="#appearance">File</a>
      <a href="#alarms">Edit</a>
      <a href="#timer">View</a>
      <a href="#sounds">Special</a>
      <span>Maclock Control</span>
    </nav>

    <main>
      <div v-if="loading" class="startup-screen" role="status">
        <div class="watch-cursor" aria-hidden="true">◷</div>
        <strong>Opening Maclock Control Panel…</strong>
      </div>

      <template v-else-if="panelState">
        <MacWindow id="welcome" title="Maclock Control Panel" wide>
          <div class="welcome-layout">
            <div class="classic-mac" aria-hidden="true">
              <div class="classic-mac__screen">
                <i></i><i></i>
                <b></b>
              </div>
              <span></span>
            </div>
            <div>
              <h1>Maclock Control Panel</h1>
              <p>
                Adjust your clock from any computer or phone on the same
                network. Changes are saved directly to Maclock.
              </p>
              <div class="connection-badge">
                <span aria-hidden="true"></span>
                Connected locally
              </div>
            </div>
          </div>
        </MacWindow>

        <div class="control-grid">
          <MacWindow id="appearance" title="Appearance">
            <form class="panel-form" @submit.prevent="saveAppearance">
              <label class="field">
                <span>Clock face</span>
                <select v-model.number="panelState.appearance.face">
                  <option
                    v-for="(name, index) in faceOptions"
                    :key="name"
                    :value="index"
                  >
                    {{ name }}
                  </option>
                </select>
              </label>

              <fieldset class="radio-box">
                <legend>Theme</legend>
                <label
                  v-for="(name, index) in themeOptions"
                  :key="name"
                  class="classic-radio"
                >
                  <input
                    v-model.number="panelState.appearance.theme"
                    type="radio"
                    :value="index"
                  />
                  <span>{{ name }}</span>
                </label>
              </fieldset>

              <label class="field">
                <span class="field-line">
                  <span>Brightness</span>
                  <output>{{ panelState.appearance.brightness }} / 12</output>
                </span>
                <input
                  v-model.number="panelState.appearance.brightness"
                  type="range"
                  min="0"
                  max="12"
                />
              </label>

              <div class="button-row">
                <MacButton
                  default-action
                  type="submit"
                  :disabled="!!busy"
                >
                  Apply
                </MacButton>
              </div>
            </form>
          </MacWindow>

          <MacWindow id="timer" title="Timer">
            <form class="panel-form" @submit.prevent="timerAction('start')">
              <div
                class="timer-display"
                :class="{ active: panelState.timer.active }"
                aria-live="polite"
              >
                {{ panelState.timer.active ? timerText : "READY" }}
              </div>

              <div class="timer-fields">
                <label class="field">
                  <span>Minutes</span>
                  <input
                    v-model.number="panelState.timer.minutes"
                    type="number"
                    min="1"
                    max="1440"
                  />
                </label>
                <label class="field">
                  <span>Volume</span>
                  <select v-model.number="panelState.timer.volume">
                    <option
                      v-for="(name, index) in volumeOptions"
                      :key="name"
                      :value="index"
                    >
                      {{ name }}
                    </option>
                  </select>
                </label>
              </div>

              <label class="field">
                <span>Sound</span>
                <div class="sound-line">
                  <select v-model="panelState.timer.sound">
                    <option
                      v-for="sound in sounds"
                      :key="sound.path"
                      :value="sound.path"
                    >
                      {{ sound.name }}
                    </option>
                  </select>
                  <MacButton
                    secondary
                    aria-label="Preview timer sound"
                    :disabled="!!busy"
                    @click="
                      previewSound(
                        panelState.timer.sound,
                        panelState.timer.volume,
                        true,
                      )
                    "
                  >
                    ▶
                  </MacButton>
                </div>
              </label>

              <div class="button-row button-row--split">
                <MacButton
                  v-if="panelState.timer.active"
                  danger
                  :disabled="!!busy"
                  @click="timerAction('cancel')"
                >
                  Cancel
                </MacButton>
                <MacButton
                  v-else
                  secondary
                  :disabled="!!busy"
                  @click="timerAction('save')"
                >
                  Save
                </MacButton>
                <MacButton
                  default-action
                  type="submit"
                  :disabled="!!busy"
                >
                  Start
                </MacButton>
              </div>
            </form>
          </MacWindow>

          <MacWindow id="alarms" title="Alarm Clock" wide>
            <div class="alarm-grid">
              <form
                v-for="(alarm, alarmIndex) in panelState.alarms"
                :key="alarmIndex"
                class="alarm-card"
                @submit.prevent="saveAlarm(alarmIndex)"
              >
                <div class="alarm-heading">
                  <strong>Alarm {{ alarmIndex + 1 }}</strong>
                  <label class="switch-label">
                    <input v-model="alarm.enabled" type="checkbox" />
                    <span>{{ alarm.enabled ? "On" : "Off" }}</span>
                  </label>
                </div>

                <div class="time-entry">
                  <label>
                    <span>Hour</span>
                    <input
                      v-model.number="alarm.hour"
                      type="number"
                      min="0"
                      max="23"
                    />
                  </label>
                  <b aria-hidden="true">:</b>
                  <label>
                    <span>Minute</span>
                    <input
                      v-model.number="alarm.minute"
                      type="number"
                      min="0"
                      max="59"
                    />
                  </label>
                </div>

                <fieldset class="weekdays">
                  <legend>Repeat</legend>
                  <button
                    v-for="(day, dayIndex) in weekdays"
                    :key="dayIndex"
                    type="button"
                    :class="{
                      selected: alarmWeekdayEnabled(alarm, dayIndex),
                    }"
                    :aria-pressed="alarmWeekdayEnabled(alarm, dayIndex)"
                    @click="toggleAlarmWeekday(alarm, dayIndex)"
                  >
                    {{ day }}
                  </button>
                </fieldset>

                <label class="field">
                  <span>Sound</span>
                  <div class="sound-line">
                    <select v-model="alarm.sound">
                      <option
                        v-for="sound in sounds"
                        :key="sound.path"
                        :value="sound.path"
                      >
                        {{ sound.name }}
                      </option>
                    </select>
                    <MacButton
                      secondary
                      aria-label="Preview alarm sound"
                      :disabled="!!busy"
                      @click="
                        previewSound(alarm.sound, alarm.volume, true)
                      "
                    >
                      ▶
                    </MacButton>
                  </div>
                </label>

                <label class="field">
                  <span>Volume</span>
                  <select v-model.number="alarm.volume">
                    <option
                      v-for="(name, index) in volumeOptions"
                      :key="name"
                      :value="index"
                    >
                      {{ name }}
                    </option>
                  </select>
                </label>

                <div class="button-row">
                  <MacButton
                    default-action
                    type="submit"
                    :disabled="!!busy"
                  >
                    Save
                  </MacButton>
                </div>
              </form>
            </div>
          </MacWindow>

          <MacWindow id="night" title="Night Mode">
            <form class="panel-form" @submit.prevent="saveNightMode">
              <label class="check-line">
                <input v-model="panelState.night.enabled" type="checkbox" />
                <span>Automatically dim the display</span>
              </label>

              <div class="two-column">
                <label class="field">
                  <span>Dim from</span>
                  <select v-model.number="panelState.night.start">
                    <option v-for="hour in 24" :key="hour" :value="hour - 1">
                      {{ String(hour - 1).padStart(2, "0") }}:00
                    </option>
                  </select>
                </label>
                <label class="field">
                  <span>Normal at</span>
                  <select v-model.number="panelState.night.end">
                    <option v-for="hour in 24" :key="hour" :value="hour - 1">
                      {{ String(hour - 1).padStart(2, "0") }}:00
                    </option>
                  </select>
                </label>
              </div>

              <hr />

              <label class="check-line">
                <input v-model="panelState.night.screenOff" type="checkbox" />
                <span>Turn the screen completely off</span>
              </label>

              <label class="field">
                <span>Screen-off time</span>
                <select
                  v-model.number="panelState.night.offHour"
                  :disabled="!panelState.night.screenOff"
                >
                  <option v-for="hour in 24" :key="hour" :value="hour - 1">
                    {{ String(hour - 1).padStart(2, "0") }}:00
                  </option>
                </select>
              </label>
              <p class="help-text">
                Touching the screen or either clock button wakes it temporarily.
              </p>

              <div class="button-row">
                <MacButton
                  default-action
                  type="submit"
                  :disabled="!!busy"
                >
                  Save
                </MacButton>
              </div>
            </form>
          </MacWindow>

          <MacWindow id="chime" title="Hourly Chime">
            <form class="panel-form" @submit.prevent="saveChime">
              <label class="field">
                <span>Schedule</span>
                <select v-model.number="panelState.chime.mode">
                  <option
                    v-for="(name, index) in chimeOptions"
                    :key="name"
                    :value="index"
                  >
                    {{ name }}
                  </option>
                </select>
              </label>

              <label class="field">
                <span>Sound</span>
                <div class="sound-line">
                  <select v-model="panelState.chime.sound">
                    <option
                      v-for="sound in sounds"
                      :key="sound.path"
                      :value="sound.path"
                    >
                      {{ sound.name }}
                    </option>
                  </select>
                  <MacButton
                    secondary
                    aria-label="Preview chime"
                    :disabled="!!busy"
                    @click="
                      previewSound(
                        panelState.chime.sound,
                        panelState.chime.volume,
                        true,
                      )
                    "
                  >
                    ▶
                  </MacButton>
                </div>
              </label>

              <label class="field">
                <span>Volume</span>
                <select v-model.number="panelState.chime.volume">
                  <option
                    v-for="(name, index) in volumeOptions"
                    :key="name"
                    :value="index"
                  >
                    {{ name }}
                  </option>
                </select>
              </label>

              <label class="check-line">
                <input v-model="panelState.chime.quiet" type="checkbox" />
                <span>Use quiet hours</span>
              </label>

              <div class="two-column">
                <label class="field">
                  <span>Quiet from</span>
                  <select v-model.number="panelState.chime.quietStart">
                    <option v-for="hour in 24" :key="hour" :value="hour - 1">
                      {{ String(hour - 1).padStart(2, "0") }}:00
                    </option>
                  </select>
                </label>
                <label class="field">
                  <span>Resume at</span>
                  <select v-model.number="panelState.chime.quietEnd">
                    <option v-for="hour in 24" :key="hour" :value="hour - 1">
                      {{ String(hour - 1).padStart(2, "0") }}:00
                    </option>
                  </select>
                </label>
              </div>

              <div class="button-row">
                <MacButton
                  default-action
                  type="submit"
                  :disabled="!!busy"
                >
                  Save
                </MacButton>
              </div>
            </form>
          </MacWindow>

          <MacWindow id="sounds" title="Sound Manager" wide>
            <form class="sound-manager" @submit.prevent="saveSystemSounds">
              <div class="speaker-icon" aria-hidden="true">
                <span></span>
              </div>

              <div class="sound-settings">
                <div class="sound-setting">
                  <strong>Startup sound</strong>
                  <label class="field">
                    <span>Sound file</span>
                    <div class="sound-line">
                      <select v-model="panelState.systemSounds.startup">
                        <option
                          v-for="sound in sounds"
                          :key="sound.path"
                          :value="sound.path"
                        >
                          {{ sound.name }}
                        </option>
                      </select>
                      <MacButton
                        secondary
                        aria-label="Preview startup sound"
                        :disabled="!!busy"
                        @click="
                          previewSound(
                            panelState.systemSounds.startup,
                            panelState.systemSounds.startupVolume,
                          )
                        "
                      >
                        ▶
                      </MacButton>
                    </div>
                  </label>
                  <label class="field">
                    <span class="field-line">
                      <span>Volume</span>
                      <output>
                        {{ panelState.systemSounds.startupVolume }}%
                      </output>
                    </span>
                    <input
                      v-model.number="panelState.systemSounds.startupVolume"
                      type="range"
                      min="0"
                      max="100"
                    />
                  </label>
                </div>

                <div class="sound-setting">
                  <strong>Floppy sound</strong>
                  <label class="field">
                    <span>Sound file</span>
                    <div class="sound-line">
                      <select v-model="panelState.systemSounds.floppy">
                        <option
                          v-for="sound in sounds"
                          :key="sound.path"
                          :value="sound.path"
                        >
                          {{ sound.name }}
                        </option>
                      </select>
                      <MacButton
                        secondary
                        aria-label="Preview floppy sound"
                        :disabled="!!busy"
                        @click="
                          previewSound(
                            panelState.systemSounds.floppy,
                            panelState.systemSounds.floppyVolume,
                          )
                        "
                      >
                        ▶
                      </MacButton>
                    </div>
                  </label>
                  <label class="field">
                    <span class="field-line">
                      <span>Volume</span>
                      <output>
                        {{ panelState.systemSounds.floppyVolume }}%
                      </output>
                    </span>
                    <input
                      v-model.number="panelState.systemSounds.floppyVolume"
                      type="range"
                      min="0"
                      max="100"
                    />
                  </label>
                </div>
              </div>

              <div class="button-row">
                <MacButton
                  default-action
                  type="submit"
                  :disabled="!!busy"
                >
                  Save Sounds
                </MacButton>
              </div>
            </form>
          </MacWindow>
        </div>

        <footer>
          <strong>Maclock Control Panel</strong>
          <span aria-hidden="true">•</span>
          This is the everyday control panel. The “Maclock Setup” Wi-Fi portal
          remains separate and is only used to join a network.
          <span aria-hidden="true">•</span>
          <a
            class="github-link"
            href="https://github.com/fensoft/maclock"
            target="_blank"
            rel="noopener noreferrer"
          >
            View on GitHub
          </a>
        </footer>
      </template>
    </main>

    <Transition name="alert">
      <div
        v-if="notice"
        class="classic-alert"
        :class="{ error: notice.kind === 'error' }"
        role="status"
      >
        <div class="alert-icon" aria-hidden="true">
          {{ notice.kind === "error" ? "!" : "✓" }}
        </div>
        <p>{{ notice.message }}</p>
        <MacButton default-action @click="notice = null">OK</MacButton>
      </div>
    </Transition>
  </div>
</template>
