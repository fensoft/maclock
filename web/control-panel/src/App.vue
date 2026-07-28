<script setup>
import {
  computed,
  nextTick,
  onBeforeUnmount,
  onMounted,
  ref,
  watch,
} from "vue";

import { fetchState, fetchStatus, postForm } from "./api";
import MacAppIcon from "./components/MacAppIcon.vue";
import MacButton from "./components/MacButton.vue";
import MacWindow from "./components/MacWindow.vue";
import {
  languageCodes,
  languageOptions,
  translate,
} from "./i18n";

const volumeLevels = [10, 20, 40, 60, 80, 100];
const volumeOptions = volumeLevels.map((volume) => `${volume}%`);
const launcherApps = [
  { id: "appearance", titleKey: "appearance", icon: "appearance" },
  { id: "location", titleKey: "location", icon: "location" },
  { id: "screensaver", titleKey: "screensaver", icon: "screensaver" },
  { id: "timer", titleKey: "timer", icon: "timer" },
  { id: "alarms", titleKey: "alarmClock", icon: "alarm" },
  { id: "night", titleKey: "nightMode", icon: "night" },
  { id: "chime", titleKey: "hourlyChime", icon: "chime" },
  { id: "sounds", titleKey: "soundManager", icon: "sound" },
];

const panelState = ref(null);
const loading = ref(true);
const busy = ref("");
const notice = ref(null);
const timerRemaining = ref(0);
const selectedApp = ref(null);
const activeApp = ref(null);
const activeAppBaseline = ref(null);
const closeConfirmation = ref(false);
const pendingTransition = ref(null);
const launcherRef = ref(null);
const activeWindowRef = ref(null);
const keepEditingRef = ref(null);
let statusPoll = 0;
let noticeTimeout = 0;

const sounds = computed(() => panelState.value?.sounds || []);
const currentLanguage = computed(
  () => Number(panelState.value?.appearance?.language) || 0,
);
const t = (key, replacements) =>
  translate(currentLanguage.value, key, replacements);
const activeAppEntry = computed(
  () => launcherApps.find((app) => app.id === activeApp.value) || null,
);
const activeAppTitle = computed(() =>
  activeAppEntry.value ? t(activeAppEntry.value.titleKey) : "",
);
const faceOptions = computed(() =>
  ["macintosh", "compactDigital", "analog", "flipClock"].map((key) => t(key)),
);
const themeOptions = computed(() => ["light", "dark"].map((key) => t(key)));
const hourFormatOptions = computed(() =>
  ["hour24", "hour12"].map((key) => t(key)),
);
const chimeOptions = computed(() =>
  ["off", "hourly", "quarterHour"].map((key) => t(key)),
);
const screensaverOptions = computed(() =>
  [
    "off",
    "afterDark",
    "starfield",
    "bouncingMac",
    "matrixRain",
    "pipes",
    "flyingClocks",
    "randomRotation",
  ].map((key) => t(key)),
);
const screensaverDelayOptions = computed(() =>
  [
    "after1Minute",
    "after5Minutes",
    "after10Minutes",
    "after30Minutes",
  ].map((key) => t(key)),
);
const weekdays = computed(() => t("weekdays"));
const timerText = computed(() => {
  const seconds = Math.max(0, timerRemaining.value);
  const hours = Math.floor(seconds / 3600);
  const minutes = Math.floor((seconds % 3600) / 60);
  const rest = seconds % 60;
  return [hours, minutes, rest]
    .map((value) => String(value).padStart(2, "0"))
    .join(":");
});

function cloneValue(value) {
  return JSON.parse(JSON.stringify(value));
}

function copyFields(source, fields) {
  return fields.reduce((result, field) => {
    result[field] = source[field];
    return result;
  }, {});
}

function editableSnapshot(appId) {
  if (!panelState.value) return null;
  switch (appId) {
    case "appearance":
      return copyFields(panelState.value.appearance, [
        "language",
        "face",
        "theme",
        "brightness",
        "hourFormat",
        "leadingZero",
        "seconds",
        "weekday",
      ]);
    case "location":
      return copyFields(panelState.value.location, ["city", "country"]);
    case "screensaver":
      return copyFields(panelState.value.screensaver, ["mode", "delay"]);
    case "timer":
      return copyFields(panelState.value.timer, [
        "minutes",
        "sound",
        "volume",
      ]);
    case "alarms":
      return panelState.value.alarms.map((alarm) =>
        copyFields(alarm, [
          "enabled",
          "hour",
          "minute",
          "weekdays",
          "sound",
          "volume",
        ]),
      );
    case "night":
      return copyFields(panelState.value.night, [
        "enabled",
        "start",
        "end",
        "screenOff",
        "offHour",
      ]);
    case "chime":
      return copyFields(panelState.value.chime, [
        "mode",
        "sound",
        "volume",
        "quiet",
        "quietStart",
        "quietEnd",
      ]);
    case "sounds":
      return copyFields(panelState.value.systemSounds, [
        "startup",
        "startupVolume",
        "floppy",
        "floppyVolume",
      ]);
    default:
      return null;
  }
}

const activeAppDirty = computed(() => {
  if (!activeApp.value || !activeAppBaseline.value) return false;
  return (
    JSON.stringify(editableSnapshot(activeApp.value)) !==
    JSON.stringify(activeAppBaseline.value)
  );
});

function captureActiveAppBaseline() {
  activeAppBaseline.value = activeApp.value
    ? cloneValue(editableSnapshot(activeApp.value))
    : null;
}

function restoreActiveAppBaseline() {
  if (!activeApp.value || !activeAppBaseline.value) return;
  const restored = cloneValue(activeAppBaseline.value);
  switch (activeApp.value) {
    case "alarms":
      panelState.value.alarms = restored;
      break;
    case "sounds":
      Object.assign(panelState.value.systemSounds, restored);
      break;
    default:
      Object.assign(panelState.value[activeApp.value], restored);
      break;
  }
}

async function focusActiveWindow() {
  await nextTick();
  activeWindowRef.value?.focusAndReveal();
}

async function focusLauncher() {
  await nextTick();
  launcherRef.value?.focus({ preventScroll: true });
  launcherRef.value?.scrollIntoView({
    behavior: window.matchMedia("(prefers-reduced-motion: reduce)").matches
      ? "auto"
      : "smooth",
    block: "start",
  });
}

function commitAppTransition({
  target,
  selection = target,
  focusHome = false,
}) {
  activeApp.value = target;
  selectedApp.value = selection;
  pendingTransition.value = null;
  closeConfirmation.value = false;
  captureActiveAppBaseline();
  if (target) {
    focusActiveWindow();
  } else if (focusHome) {
    focusLauncher();
  }
}

async function requestAppTransition(transition) {
  if (busy.value) return;
  if (transition.target === activeApp.value) {
    selectedApp.value = transition.selection;
    if (transition.target) {
      focusActiveWindow();
    } else if (transition.focusHome) {
      focusLauncher();
    }
    return;
  }
  if (activeApp.value && activeAppDirty.value) {
    pendingTransition.value = transition;
    closeConfirmation.value = true;
    await nextTick();
    keepEditingRef.value?.focus();
    return;
  }
  commitAppTransition(transition);
}

function selectApp(appId) {
  selectedApp.value = appId;
}

function openApp(appId) {
  requestAppTransition({
    target: appId,
    selection: appId,
    focusHome: false,
  });
}

function closeActiveApp(clearSelection = false) {
  if (!activeApp.value) {
    if (clearSelection) selectedApp.value = null;
    focusLauncher();
    return;
  }
  requestAppTransition({
    target: null,
    selection: clearSelection ? null : activeApp.value,
    focusHome: true,
  });
}

function keepEditing() {
  closeConfirmation.value = false;
  pendingTransition.value = null;
  selectedApp.value = activeApp.value;
  focusActiveWindow();
}

function discardChanges() {
  restoreActiveAppBaseline();
  const transition = pendingTransition.value || {
    target: null,
    selection: activeApp.value,
    focusHome: true,
  };
  commitAppTransition(transition);
}

function handleGlobalKeydown(event) {
  if (event.key !== "Escape") return;
  event.preventDefault();
  if (closeConfirmation.value) {
    keepEditing();
  } else if (activeApp.value) {
    closeActiveApp();
  }
}

function handleGlobalPointerDown(event) {
  if (!activeApp.value || closeConfirmation.value) return;
  if (!(event.target instanceof Element)) return;
  if (event.target.closest(".active-window-slot > .mac-window")) return;
  if (event.target.closest(".menu-bar")) return;
  closeActiveApp();
}

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
    captureActiveAppBaseline();
  } catch (error) {
    showNotice(t("contactError"), "error");
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
    await postForm(path, values);
    showNotice(successMessage);
    if (refresh) await loadState({ quiet: true });
  } catch (error) {
    showNotice(t("saveError"), "error");
  } finally {
    busy.value = "";
  }
}

function saveAppearance() {
  const appearance = panelState.value.appearance;
  runAction(
    "appearance",
    "/api/appearance",
    {
      ...appearance,
      leadingZero: appearance.leadingZero ? 1 : 0,
      seconds: appearance.seconds ? 1 : 0,
      weekday: appearance.weekday ? 1 : 0,
    },
    t("appearanceSaved"),
  );
}

function saveLocation() {
  const location = panelState.value.location;
  location.country = String(location.country || "")
    .trim()
    .toUpperCase();
  runAction(
    "location",
    "/api/location",
    {
      city: location.city,
      country: location.country,
    },
    t("locationSaved"),
  );
}

function screensaverAction(action) {
  const screensaver = panelState.value.screensaver;
  runAction(
    `screensaver-${action}`,
    "/api/screensaver",
    {
      action,
      mode: screensaver.mode,
      delay: screensaver.delay,
    },
    action === "launch"
      ? t("screensaverLaunched")
      : t("screensaverSaved"),
  );
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
    t("alarmSaved", { number: index + 1 }),
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
    action === "start" ? t("timerStarted") : t("timerSaved"),
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
    t("nightSaved"),
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
    t("chimeSaved"),
  );
}

function saveSystemSounds() {
  const system = panelState.value.systemSounds;
  runAction(
    "sounds",
    "/api/sounds",
    system,
    t("soundsSaved"),
  );
}

function previewSound(sound, volume, levelScale = false) {
  const previewVolume = levelScale
    ? volumeLevels[Number(volume)] || 80
    : Number(volume);
  runAction(
    `preview-${sound}`,
    "/api/preview",
    { sound, volume: previewVolume },
    t("playing"),
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
    panelState.value.screensaver.active =
      status.screensaver.active;
  } catch {
    // Keep the last known timer state during a transient Wi-Fi interruption.
  }
}

onMounted(async () => {
  await loadState();
  statusPoll = window.setInterval(pollStatus, 3000);
  window.addEventListener("keydown", handleGlobalKeydown);
  window.addEventListener("pointerdown", handleGlobalPointerDown, true);
});

watch(
  currentLanguage,
  (language) => {
    document.documentElement.lang = languageCodes[language] || "en";
  },
  { immediate: true },
);

onBeforeUnmount(() => {
  window.clearInterval(statusPoll);
  window.clearTimeout(noticeTimeout);
  window.removeEventListener("keydown", handleGlobalKeydown);
  window.removeEventListener("pointerdown", handleGlobalPointerDown, true);
});
</script>

<template>
  <div class="desktop">
    <nav class="menu-bar" :aria-label="t('menuSections')">
      <button
        class="apple"
        type="button"
        :aria-label="t('home')"
        @click="closeActiveApp(true)"
      >
        <svg viewBox="0 0 24 24" aria-hidden="true">
          <path
            d="M17.05 20.28c-.98.95-2.05.8-3.08.35-1.09-.46-2.09-.48-3.24 0-1.44.62-2.2.44-3.06-.35C2.79 15.25 3.51 7.59 9.05 7.31c1.35-.07 2.29.74 3.08.79 1.18-.24 2.31-.93 3.57-.84 1.51.12 2.65.72 3.4 1.8-3.12 1.87-2.38 5.98.48 7.13-.57 1.5-1.31 2.99-2.53 4.09M12.03 7.25C11.88 5.02 13.69 3.18 15.77 3c.29 2.58-2.34 4.5-3.74 4.25"
          />
        </svg>
      </button>
      <button type="button" @click="openApp('appearance')">
        {{ t("file") }}
      </button>
      <button type="button" @click="openApp('alarms')">
        {{ t("edit") }}
      </button>
      <button type="button" @click="openApp('screensaver')">
        {{ t("view") }}
      </button>
      <button type="button" @click="openApp('sounds')">
        {{ t("special") }}
      </button>
      <span>{{ t("control") }}</span>
    </nav>

    <main>
      <div v-if="loading" class="startup-screen" role="status">
        <div class="watch-cursor" aria-hidden="true">◷</div>
        <strong>{{ t("opening") }}</strong>
      </div>

      <template v-else-if="panelState">
        <MacWindow id="welcome" :title="t('panelTitle')" wide>
          <div class="welcome-layout">
            <div class="classic-mac" aria-hidden="true">
              <div class="classic-mac__screen">
                <i></i><i></i>
                <b></b>
              </div>
              <span></span>
            </div>
            <div>
              <h1>{{ t("panelTitle") }}</h1>
              <p>{{ t("welcome") }}</p>
              <div class="connection-badge">
                <span aria-hidden="true"></span>
                {{ t("connected") }}
              </div>
            </div>
          </div>
        </MacWindow>

        <section
          v-if="!activeApp"
          ref="launcherRef"
          class="app-launcher"
          tabindex="-1"
          :aria-label="t('launcherTitle')"
        >
          <div class="app-grid">
            <MacAppIcon
              v-for="app in launcherApps"
              :key="app.id"
              :app-id="app.id"
              :icon="app.icon"
              :title="t(app.titleKey)"
              :open-label="
                t('openApp', { title: t(app.titleKey) })
              "
              :selected="selectedApp === app.id"
              @select="selectApp(app.id)"
              @open="openApp(app.id)"
            />
          </div>
        </section>

        <div v-if="activeApp" class="active-window-slot">
          <MacWindow
            v-if="activeApp === 'appearance'"
            id="appearance"
            ref="activeWindowRef"
            :title="activeAppTitle"
            closable
            :close-label="
              t('closeWindow', { title: activeAppTitle })
            "
            @close="closeActiveApp()"
          >
            <form class="panel-form" @submit.prevent="saveAppearance">
              <label class="field">
                <span>{{ t("language") }}</span>
                <select v-model.number="panelState.appearance.language">
                  <option
                    v-for="(name, index) in languageOptions"
                    :key="name"
                    :value="index"
                  >
                    {{ name }}
                  </option>
                </select>
              </label>

              <label class="field">
                <span>{{ t("clockFace") }}</span>
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
                <legend>{{ t("theme") }}</legend>
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

              <fieldset class="radio-box">
                <legend>{{ t("hourFormat") }}</legend>
                <label
                  v-for="(name, index) in hourFormatOptions"
                  :key="name"
                  class="classic-radio"
                >
                  <input
                    v-model.number="panelState.appearance.hourFormat"
                    type="radio"
                    :value="index"
                  />
                  <span>{{ name }}</span>
                </label>
              </fieldset>

              <label class="check-line">
                <input
                  v-model="panelState.appearance.leadingZero"
                  type="checkbox"
                />
                <span>{{ t("leadingZero") }}</span>
              </label>

              <label class="check-line">
                <input
                  v-model="panelState.appearance.seconds"
                  type="checkbox"
                />
                <span>{{ t("showSeconds") }}</span>
              </label>

              <label class="check-line">
                <input
                  v-model="panelState.appearance.weekday"
                  type="checkbox"
                />
                <span>{{ t("showWeekday") }}</span>
              </label>

              <label class="field">
                <span class="field-line">
                  <span>{{ t("brightness") }}</span>
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
                  {{ t("apply") }}
                </MacButton>
              </div>
            </form>
          </MacWindow>

          <MacWindow
            v-if="activeApp === 'location'"
            id="location"
            ref="activeWindowRef"
            :title="activeAppTitle"
            closable
            :close-label="
              t('closeWindow', { title: activeAppTitle })
            "
            @close="closeActiveApp()"
          >
            <form class="panel-form" @submit.prevent="saveLocation">
              <div class="two-column location-fields">
                <label class="field">
                  <span>{{ t("city") }}</span>
                  <input
                    v-model.trim="panelState.location.city"
                    type="text"
                    maxlength="48"
                    autocomplete="address-level2"
                    required
                  />
                </label>

                <label class="field">
                  <span>{{ t("countryCode") }}</span>
                  <input
                    v-model.trim="panelState.location.country"
                    type="text"
                    maxlength="2"
                    pattern="[A-Za-z]{2}"
                    autocomplete="country"
                    placeholder="FR"
                  />
                </label>
              </div>

              <p class="help-text">{{ t("countryHelp") }}</p>

              <dl class="location-summary">
                <div>
                  <dt>{{ t("resolvedLocation") }}</dt>
                  <dd>
                    {{
                      panelState.location.resolved ||
                      panelState.location.city
                    }}
                  </dd>
                </div>
                <div>
                  <dt>{{ t("timezone") }}</dt>
                  <dd>
                    {{ panelState.location.timezone || t("updating") }}
                  </dd>
                </div>
              </dl>

              <div class="button-row">
                <MacButton
                  default-action
                  type="submit"
                  :disabled="!!busy"
                >
                  {{ t("apply") }}
                </MacButton>
              </div>
            </form>
          </MacWindow>

          <MacWindow
            v-if="activeApp === 'screensaver'"
            id="screensaver"
            ref="activeWindowRef"
            :title="activeAppTitle"
            closable
            :close-label="
              t('closeWindow', { title: activeAppTitle })
            "
            @close="closeActiveApp()"
          >
            <form
              class="panel-form"
              @submit.prevent="screensaverAction('save')"
            >
              <div
                class="saver-status"
                :class="{ active: panelState.screensaver.active }"
                role="status"
              >
                <span aria-hidden="true"></span>
                {{
                  panelState.screensaver.active
                    ? t("screensaverRunning")
                    : t("screensaverReady")
                }}
              </div>

              <fieldset class="radio-box screensaver-modes">
                <legend>{{ t("screensaverMode") }}</legend>
                <label
                  v-for="(name, index) in screensaverOptions"
                  :key="name"
                  class="classic-radio"
                >
                  <input
                    v-model.number="panelState.screensaver.mode"
                    type="radio"
                    :value="index"
                  />
                  <span>{{ name }}</span>
                </label>
              </fieldset>

              <label class="field">
                <span>{{ t("startAfter") }}</span>
                <select v-model.number="panelState.screensaver.delay">
                  <option
                    v-for="(name, index) in screensaverDelayOptions"
                    :key="name"
                    :value="index"
                  >
                    {{ name }}
                  </option>
                </select>
              </label>

              <p class="help-text">{{ t("screensaverHelp") }}</p>

              <div class="button-row button-row--split">
                <MacButton
                  secondary
                  type="submit"
                  :disabled="!!busy"
                >
                  {{ t("save") }}
                </MacButton>
                <MacButton
                  default-action
                  :disabled="
                    !!busy || panelState.screensaver.mode === 0
                  "
                  @click="screensaverAction('launch')"
                >
                  {{ t("launchNow") }}
                </MacButton>
              </div>
            </form>
          </MacWindow>

          <MacWindow
            v-if="activeApp === 'timer'"
            id="timer"
            ref="activeWindowRef"
            :title="activeAppTitle"
            closable
            :close-label="
              t('closeWindow', { title: activeAppTitle })
            "
            @close="closeActiveApp()"
          >
            <form class="panel-form" @submit.prevent="timerAction('start')">
              <div
                class="timer-display"
                :class="{ active: panelState.timer.active }"
                aria-live="polite"
              >
                {{ panelState.timer.active ? timerText : t("ready") }}
              </div>

              <div class="timer-fields">
                <label class="field">
                  <span>{{ t("minutes") }}</span>
                  <input
                    v-model.number="panelState.timer.minutes"
                    type="number"
                    min="1"
                    max="1440"
                  />
                </label>
                <label class="field">
                  <span>{{ t("volume") }}</span>
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
                <span>{{ t("sound") }}</span>
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
                    :aria-label="t('previewTimer')"
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
                  {{ t("cancel") }}
                </MacButton>
                <MacButton
                  v-else
                  secondary
                  :disabled="!!busy"
                  @click="timerAction('save')"
                >
                  {{ t("save") }}
                </MacButton>
                <MacButton
                  default-action
                  type="submit"
                  :disabled="!!busy"
                >
                  {{ t("start") }}
                </MacButton>
              </div>
            </form>
          </MacWindow>

          <MacWindow
            v-if="activeApp === 'alarms'"
            id="alarms"
            ref="activeWindowRef"
            :title="activeAppTitle"
            closable
            wide
            :close-label="
              t('closeWindow', { title: activeAppTitle })
            "
            @close="closeActiveApp()"
          >
            <div class="alarm-grid">
              <form
                v-for="(alarm, alarmIndex) in panelState.alarms"
                :key="alarmIndex"
                class="alarm-card"
                @submit.prevent="saveAlarm(alarmIndex)"
              >
                <div class="alarm-heading">
                  <strong>{{ t("alarm") }} {{ alarmIndex + 1 }}</strong>
                  <label class="switch-label">
                    <input v-model="alarm.enabled" type="checkbox" />
                    <span>{{ alarm.enabled ? t("on") : t("off") }}</span>
                  </label>
                </div>

                <div class="time-entry">
                  <label>
                    <span>{{ t("hour") }}</span>
                    <input
                      v-model.number="alarm.hour"
                      type="number"
                      min="0"
                      max="23"
                    />
                  </label>
                  <b aria-hidden="true">:</b>
                  <label>
                    <span>{{ t("minute") }}</span>
                    <input
                      v-model.number="alarm.minute"
                      type="number"
                      min="0"
                      max="59"
                    />
                  </label>
                </div>

                <fieldset class="weekdays">
                  <legend>{{ t("repeat") }}</legend>
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
                  <span>{{ t("sound") }}</span>
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
                      :aria-label="t('previewAlarm')"
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
                  <span>{{ t("volume") }}</span>
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
                    {{ t("save") }}
                  </MacButton>
                </div>
              </form>
            </div>
          </MacWindow>

          <MacWindow
            v-if="activeApp === 'night'"
            id="night"
            ref="activeWindowRef"
            :title="activeAppTitle"
            closable
            :close-label="
              t('closeWindow', { title: activeAppTitle })
            "
            @close="closeActiveApp()"
          >
            <form class="panel-form" @submit.prevent="saveNightMode">
              <label class="check-line">
                <input v-model="panelState.night.enabled" type="checkbox" />
                <span>{{ t("automaticDim") }}</span>
              </label>

              <div class="two-column">
                <label class="field">
                  <span>{{ t("dimFrom") }}</span>
                  <select v-model.number="panelState.night.start">
                    <option v-for="hour in 24" :key="hour" :value="hour - 1">
                      {{ String(hour - 1).padStart(2, "0") }}:00
                    </option>
                  </select>
                </label>
                <label class="field">
                  <span>{{ t("normalAt") }}</span>
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
                <span>{{ t("screenOff") }}</span>
              </label>

              <label class="field">
                <span>{{ t("screenOffTime") }}</span>
                <select
                  v-model.number="panelState.night.offHour"
                  :disabled="!panelState.night.screenOff"
                >
                  <option v-for="hour in 24" :key="hour" :value="hour - 1">
                    {{ String(hour - 1).padStart(2, "0") }}:00
                  </option>
                </select>
              </label>
              <p class="help-text">{{ t("wakeHelp") }}</p>

              <div class="button-row">
                <MacButton
                  default-action
                  type="submit"
                  :disabled="!!busy"
                >
                  {{ t("save") }}
                </MacButton>
              </div>
            </form>
          </MacWindow>

          <MacWindow
            v-if="activeApp === 'chime'"
            id="chime"
            ref="activeWindowRef"
            :title="activeAppTitle"
            closable
            :close-label="
              t('closeWindow', { title: activeAppTitle })
            "
            @close="closeActiveApp()"
          >
            <form class="panel-form" @submit.prevent="saveChime">
              <label class="field">
                <span>{{ t("schedule") }}</span>
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
                <span>{{ t("sound") }}</span>
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
                    :aria-label="t('previewChime')"
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
                <span>{{ t("volume") }}</span>
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
                <span>{{ t("quietHours") }}</span>
              </label>

              <div class="two-column">
                <label class="field">
                  <span>{{ t("quietFrom") }}</span>
                  <select v-model.number="panelState.chime.quietStart">
                    <option v-for="hour in 24" :key="hour" :value="hour - 1">
                      {{ String(hour - 1).padStart(2, "0") }}:00
                    </option>
                  </select>
                </label>
                <label class="field">
                  <span>{{ t("resumeAt") }}</span>
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
                  {{ t("save") }}
                </MacButton>
              </div>
            </form>
          </MacWindow>

          <MacWindow
            v-if="activeApp === 'sounds'"
            id="sounds"
            ref="activeWindowRef"
            :title="activeAppTitle"
            closable
            wide
            :close-label="
              t('closeWindow', { title: activeAppTitle })
            "
            @close="closeActiveApp()"
          >
            <form class="sound-manager" @submit.prevent="saveSystemSounds">
              <div class="speaker-icon" aria-hidden="true">
                <span></span>
              </div>

              <div class="sound-settings">
                <div class="sound-setting">
                  <strong>{{ t("startupSound") }}</strong>
                  <label class="field">
                    <span>{{ t("soundFile") }}</span>
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
                        :aria-label="t('previewStartup')"
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
                      <span>{{ t("volume") }}</span>
                      <output>
                        {{ panelState.systemSounds.startupVolume }}%
                      </output>
                    </span>
                    <select
                      v-model.number="panelState.systemSounds.startupVolume"
                    >
                      <option
                        v-for="volume in volumeLevels"
                        :key="volume"
                        :value="volume"
                      >
                        {{ volume }}%
                      </option>
                    </select>
                  </label>
                </div>

                <div class="sound-setting">
                  <strong>{{ t("floppySound") }}</strong>
                  <label class="field">
                    <span>{{ t("soundFile") }}</span>
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
                        :aria-label="t('previewFloppy')"
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
                      <span>{{ t("volume") }}</span>
                      <output>
                        {{ panelState.systemSounds.floppyVolume }}%
                      </output>
                    </span>
                    <select
                      v-model.number="panelState.systemSounds.floppyVolume"
                    >
                      <option
                        v-for="volume in volumeLevels"
                        :key="volume"
                        :value="volume"
                      >
                        {{ volume }}%
                      </option>
                    </select>
                  </label>
                </div>
              </div>

              <div class="button-row">
                <MacButton
                  default-action
                  type="submit"
                  :disabled="!!busy"
                >
                  {{ t("saveSounds") }}
                </MacButton>
              </div>
            </form>
          </MacWindow>
        </div>

        <footer class="corner-footer">
          <a
            class="github-link"
            href="https://github.com/fensoft/maclock"
            target="_blank"
            rel="noopener noreferrer"
            aria-label="GitHub"
          >
            github
          </a>
        </footer>
      </template>
    </main>

    <Transition name="dialog">
      <div
        v-if="closeConfirmation"
        class="dialog-shade"
        @click.self="keepEditing"
      >
        <section
          class="classic-confirm"
          role="alertdialog"
          aria-modal="true"
          aria-labelledby="unsaved-title"
          aria-describedby="unsaved-message"
        >
          <div class="confirm-icon" aria-hidden="true">!</div>
          <div>
            <h2 id="unsaved-title">{{ t("unsavedTitle") }}</h2>
            <p id="unsaved-message">
              {{
                t("unsavedMessage", {
                  title: activeAppTitle,
                })
              }}
            </p>
          </div>
          <div class="confirm-actions">
            <MacButton danger @click="discardChanges">
              {{ t("discardChanges") }}
            </MacButton>
            <MacButton
              ref="keepEditingRef"
              default-action
              @click="keepEditing"
            >
              {{ t("keepEditing") }}
            </MacButton>
          </div>
        </section>
      </div>
    </Transition>

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
