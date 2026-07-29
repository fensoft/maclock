<script setup>
import {
  computed,
  nextTick,
  onBeforeUnmount,
  onMounted,
  ref,
  watch,
} from "vue";

import {
  deleteSound,
  exportConfiguration,
  fetchState,
  fetchStatus,
  importSoundUrl,
  postForm,
  restoreConfiguration,
  searchMyInstants as searchMyInstantsApi,
  uploadSound,
  uploadFirmware,
} from "./api";
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
  { id: "update", titleKey: "softwareUpdate", icon: "update" },
  { id: "backup", titleKey: "configurationBackup", icon: "backup" },
];

const panelState = ref(null);
const loading = ref(true);
const busy = ref("");
const notice = ref(null);
const timerRemaining = ref(0);
const selectedApp = ref(null);
const activeApp = ref(null);
const activeAppBaseline = ref(null);
const launcherRef = ref(null);
const activeWindowRef = ref(null);
const soundFileInput = ref(null);
const firmwareFileInput = ref(null);
const backupFileInput = ref(null);
const soundDragActive = ref(false);
const soundImportUrl = ref("");
const myInstantsQuery = ref("");
const myInstantsResults = ref([]);
const myInstantsSearched = ref(false);
const deleteSoundTarget = ref(null);
const restoreConfirmation = ref(false);
const pendingBackupFile = ref(null);
let statusPoll = 0;
let noticeTimeout = 0;
let autoSaveTimeout = 0;
let myInstantsAudio = null;

const sounds = computed(() => panelState.value?.sounds || []);
const downloadedSounds = computed(() =>
  sounds.value.filter((sound) => sound.downloaded),
);
const downloadedSoundBytes = computed(() =>
  downloadedSounds.value.reduce(
    (total, sound) => total + (Number(sound.size) || 0),
    0,
  ),
);
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
const accentSwatches = [
  "linear-gradient(135deg, #fff 0 50%, #000 50%)",
  "#c62828",
  "#b45309",
  "#15803d",
  "#1d4ed8",
  "#7e22ce",
];
const accentOptions = computed(() =>
  ["defaultOption", "red", "orange", "green", "blue", "purple"].map(
    (key, index) => ({
      name: t(key),
      swatch: accentSwatches[index],
    }),
  ),
);
const numeralSizeOptions = computed(() =>
  ["small", "defaultOption", "large"].map((key) => t(key)),
);
const flipSpeedOptions = computed(() =>
  ["slow", "normal", "fast"].map((key) => t(key)),
);
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
const upcomingAlarmText = computed(() => {
  const upcoming = panelState.value?.upcomingAlarm;
  if (!upcoming?.valid) return t("noUpcomingAlarm");
  const label =
    String(upcoming.label || "").trim() ||
    `${t("alarm")} ${Number(upcoming.index) + 1}`;
  let day = t("today");
  if (Number(upcoming.dayOffset) === 1) {
    day = t("tomorrow");
  } else if (Number(upcoming.dayOffset) > 1) {
    day = t("weekdayNames")[Number(upcoming.weekday)] || "";
  }
  const time = `${String(upcoming.hour).padStart(2, "0")}:${String(
    upcoming.minute,
  ).padStart(2, "0")}`;
  const schedule = upcoming.snoozed
    ? t("snoozed")
    : upcoming.oneTime
      ? t("oneTime")
      : day;
  return `${t("nextAlarm")}: ${label} — ${schedule}, ${time}`;
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
        "accent",
        "fontSize",
        "weather",
        "flipSpeed",
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
          "oneTime",
          "gradualVolume",
          "sunrise",
          "label",
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
    case "update":
    case "backup":
      return null;
    default:
      return null;
  }
}

function captureActiveAppBaseline() {
  activeAppBaseline.value = activeApp.value
    ? cloneValue(editableSnapshot(activeApp.value))
    : null;
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
  captureActiveAppBaseline();
  if (target) {
    focusActiveWindow();
  } else if (focusHome) {
    focusLauncher();
  }
}

function requestAppTransition(transition) {
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

function handleGlobalKeydown(event) {
  if (event.key !== "Escape") return;
  event.preventDefault();
  if (deleteSoundTarget.value) {
    deleteSoundTarget.value = null;
  } else if (restoreConfirmation.value) {
    cancelConfigurationRestore();
  } else if (activeApp.value) {
    closeActiveApp();
  }
}

function handleGlobalPointerDown(event) {
  if (
    !activeApp.value ||
    deleteSoundTarget.value ||
    restoreConfirmation.value
  ) {
    return;
  }
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
      weather: appearance.weather ? 1 : 0,
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
  return runAction(
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
      oneTime: alarm.oneTime ? 1 : 0,
      gradualVolume: alarm.gradualVolume ? 1 : 0,
      sunrise: alarm.sunrise ? 1 : 0,
      label: alarm.label || "",
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

async function autoSaveApp(appId, baseline) {
  if (!appId || !panelState.value) return;
  if (busy.value) {
    scheduleAutoSave(appId, baseline);
    return;
  }
  const snapshot = editableSnapshot(appId);
  if (
    !snapshot ||
    JSON.stringify(snapshot) === JSON.stringify(baseline)
  ) {
    return;
  }
  switch (appId) {
    case "appearance":
      saveAppearance();
      break;
    case "location":
      saveLocation();
      break;
    case "screensaver":
      screensaverAction("save");
      break;
    case "timer":
      timerAction("save");
      break;
    case "alarms":
      for (const [index, alarm] of panelState.value.alarms.entries()) {
        if (
          JSON.stringify(alarm) !==
          JSON.stringify(baseline?.[index])
        ) {
          await saveAlarm(index);
        }
      }
      break;
    case "night":
      saveNightMode();
      break;
    case "chime":
      saveChime();
      break;
    case "sounds":
      saveSystemSounds();
      break;
  }
}

function scheduleAutoSave(
  appId = activeApp.value,
  baseline = cloneValue(activeAppBaseline.value),
) {
  window.clearTimeout(autoSaveTimeout);
  autoSaveTimeout = window.setTimeout(
    () => autoSaveApp(appId, baseline),
    500,
  );
}

watch(
  () => (activeApp.value ? editableSnapshot(activeApp.value) : null),
  (snapshot) => {
    if (
      !snapshot ||
      !activeAppBaseline.value ||
      JSON.stringify(snapshot) === JSON.stringify(activeAppBaseline.value)
    ) {
      return;
    }
    scheduleAutoSave();
  },
  { deep: true },
);

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

function formatSoundSize(bytes) {
  const size = Number(bytes) || 0;
  if (size < 1024) return `${size} B`;
  if (size < 1024 * 1024) return `${Math.round(size / 1024)} KB`;
  return `${(size / (1024 * 1024)).toFixed(1)} MB`;
}

async function refreshSoundLibrary(successMessage, draft) {
  await loadState({ quiet: true });
  if (draft && panelState.value?.systemSounds) {
    Object.assign(panelState.value.systemSounds, draft);
  }
  showNotice(successMessage);
}

async function addSoundFile(file) {
  if (!file || busy.value) return;
  if (!/\.mp3$/i.test(file.name || "")) {
    showNotice(t("mp3Only"), "error");
    return;
  }
  if (file.size > 6 * 1024 * 1024) {
    showNotice(t("mp3TooLarge"), "error");
    return;
  }
  const draft = cloneValue(panelState.value.systemSounds);
  busy.value = "sound-upload";
  try {
    await uploadSound(file);
    await refreshSoundLibrary(t("soundUploaded"), draft);
  } catch {
    showNotice(t("soundOperationError"), "error");
  } finally {
    busy.value = "";
    soundDragActive.value = false;
    if (soundFileInput.value) soundFileInput.value.value = "";
  }
}

function chooseSoundFile(event) {
  addSoundFile(event.target.files?.[0]);
}

function dropSoundFile(event) {
  soundDragActive.value = false;
  addSoundFile(event.dataTransfer?.files?.[0]);
}

async function importSound() {
  const url = soundImportUrl.value.trim();
  if (!url || busy.value) return;
  const draft = cloneValue(panelState.value.systemSounds);
  busy.value = "sound-import";
  try {
    await importSoundUrl(url);
    soundImportUrl.value = "";
    await refreshSoundLibrary(t("soundImported"), draft);
  } catch {
    showNotice(t("soundOperationError"), "error");
  } finally {
    busy.value = "";
  }
}

function stopMyInstantsPreview() {
  if (!myInstantsAudio) return;
  myInstantsAudio.pause();
  myInstantsAudio.src = "";
  myInstantsAudio = null;
}

function previewMyInstants(result) {
  stopMyInstantsPreview();
  myInstantsAudio = new Audio(result.mp3Url);
  myInstantsAudio.volume = 0.7;
  myInstantsAudio.addEventListener(
    "ended",
    () => {
      myInstantsAudio = null;
    },
    { once: true },
  );
  myInstantsAudio.play().catch(() => {
    stopMyInstantsPreview();
    showNotice(t("myInstantsPreviewError"), "error");
  });
}

async function searchMyInstants() {
  const query = myInstantsQuery.value.trim();
  if (query.length < 2 || busy.value) return;
  stopMyInstantsPreview();
  busy.value = "myinstants-search";
  myInstantsSearched.value = false;
  try {
    const response = await searchMyInstantsApi(query);
    myInstantsResults.value = response.results || [];
    myInstantsSearched.value = true;
  } catch {
    myInstantsResults.value = [];
    showNotice(t("myInstantsSearchError"), "error");
  } finally {
    busy.value = "";
  }
}

async function importMyInstantsResult(result) {
  if (!result || busy.value) return;
  const draft = cloneValue(panelState.value.systemSounds);
  busy.value = `myinstants-import-${result.mp3Url}`;
  try {
    await importSoundUrl(result.mp3Url, result.name);
    await refreshSoundLibrary(t("soundImported"), draft);
  } catch {
    showNotice(t("soundOperationError"), "error");
  } finally {
    busy.value = "";
  }
}

function requestSoundDeletion(sound) {
  if (!sound.downloaded || sound.inUse || busy.value) return;
  deleteSoundTarget.value = sound;
}

async function confirmSoundDeletion() {
  const target = deleteSoundTarget.value;
  if (!target || busy.value) return;
  busy.value = "sound-delete";
  try {
    await deleteSound(target.path);
    deleteSoundTarget.value = null;
    await loadState({ quiet: true });
    showNotice(t("soundDeleted"));
  } catch {
    showNotice(t("soundOperationError"), "error");
  } finally {
    busy.value = "";
  }
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
    if (status.upcomingAlarm) {
      panelState.value.upcomingAlarm = status.upcomingAlarm;
    }
    if (status.update) {
      panelState.value.update = status.update;
    }
  } catch {
    // Keep the last known timer state during a transient Wi-Fi interruption.
  }
}

async function checkForUpdates() {
  await runAction(
    "update-check",
    "/api/update/check",
    {},
    t("updateCheckStarted"),
    { refresh: false },
  );
  window.setTimeout(() => loadState({ quiet: true }), 1200);
}

async function installUpdate() {
  await runAction(
    "update-install",
    "/api/update/install",
    {},
    t("updateInstallStarted"),
    { refresh: false },
  );
}

async function dismissUpdate(action) {
  await runAction(
    `update-${action}`,
    "/api/update/dismiss",
    { action },
    t(action === "ignore" ? "updateIgnored" : "updateLater"),
  );
}

async function chooseFirmware(event) {
  const file = event.target.files?.[0];
  if (!file || busy.value) return;
  if (!/\.bin$/i.test(file.name || "")) {
    showNotice(t("firmwareBinOnly"), "error");
    return;
  }
  busy.value = "firmware-upload";
  try {
    await uploadFirmware(file);
    await loadState({ quiet: true });
    showNotice(t("firmwareUploaded"));
  } catch {
    showNotice(t("firmwareUploadError"), "error");
  } finally {
    busy.value = "";
    if (firmwareFileInput.value) {
      firmwareFileInput.value.value = "";
    }
  }
}

async function rebootForUpdate() {
  await runAction(
    "update-reboot",
    "/api/update/reboot",
    {},
    t("rebooting"),
    { refresh: false },
  );
}

async function downloadConfigurationBackup() {
  if (busy.value) return;
  busy.value = "configuration-export";
  try {
    const blob = await exportConfiguration();
    const now = new Date();
    const twoDigits = (value) => String(value).padStart(2, "0");
    const timestamp = [
      now.getFullYear(),
      twoDigits(now.getMonth() + 1),
      twoDigits(now.getDate()),
    ].join("-") +
      "_" +
      [
        twoDigits(now.getHours()),
        twoDigits(now.getMinutes()),
        twoDigits(now.getSeconds()),
      ].join("-");
    const url = blob
      ? URL.createObjectURL(blob)
      : "/api/configuration/export";
    const link = document.createElement("a");
    link.href = url;
    link.download = `maclock-backup-${timestamp}.zip`;
    document.body.appendChild(link);
    link.click();
    link.remove();
    if (blob) {
      window.setTimeout(() => URL.revokeObjectURL(url), 1000);
    }
    showNotice(t("backupDownloaded"));
  } catch {
    showNotice(t("backupExportError"), "error");
  } finally {
    busy.value = "";
  }
}

function chooseConfigurationBackup(event) {
  const file = event.target.files?.[0];
  if (!file || busy.value) return;
  if (!/\.zip$/i.test(file.name || "")) {
    showNotice(t("backupZipOnly"), "error");
    event.target.value = "";
    return;
  }
  pendingBackupFile.value = file;
  restoreConfirmation.value = true;
}

function cancelConfigurationRestore() {
  restoreConfirmation.value = false;
  pendingBackupFile.value = null;
  if (backupFileInput.value) {
    backupFileInput.value.value = "";
  }
}

async function confirmConfigurationRestore() {
  const file = pendingBackupFile.value;
  if (!file || busy.value) return;
  busy.value = "configuration-import";
  try {
    const result = await restoreConfiguration(file);
    restoreConfirmation.value = false;
    pendingBackupFile.value = null;
    if (!result.networkChanged) {
      await loadState({ quiet: true });
    }
    const warnings = Array.isArray(result.warnings)
      ? result.warnings.join(" ")
      : "";
    showNotice(
      [
        result.networkChanged
          ? t("backupRestoredReconnect")
          : t("backupRestored"),
        warnings,
      ]
        .filter(Boolean)
        .join(" "),
    );
  } catch {
    showNotice(t("backupRestoreError"), "error");
  } finally {
    busy.value = "";
    if (backupFileInput.value) {
      backupFileInput.value.value = "";
    }
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

watch(activeApp, (app) => {
  if (app !== "sounds") stopMyInstantsPreview();
});

onBeforeUnmount(() => {
  stopMyInstantsPreview();
  window.clearInterval(statusPoll);
  window.clearTimeout(noticeTimeout);
  window.clearTimeout(autoSaveTimeout);
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
            <form class="panel-form" @submit.prevent>
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

              <fieldset class="accent-box">
                <legend>{{ t("accentColor") }}</legend>
                <label
                  v-for="(accent, index) in accentOptions"
                  :key="accent.name"
                  class="accent-option"
                >
                  <input
                    v-model.number="panelState.appearance.accent"
                    type="radio"
                    :value="index"
                  />
                  <span
                    class="accent-swatch"
                    :style="{ background: accent.swatch }"
                    aria-hidden="true"
                  ></span>
                  <span>{{ accent.name }}</span>
                </label>
              </fieldset>

              <fieldset class="radio-box">
                <legend>{{ t("numeralSize") }}</legend>
                <label
                  v-for="(name, index) in numeralSizeOptions"
                  :key="name"
                  class="classic-radio"
                >
                  <input
                    v-model.number="panelState.appearance.fontSize"
                    type="radio"
                    :value="index"
                  />
                  <span>{{ name }}</span>
                </label>
              </fieldset>

              <fieldset class="radio-box">
                <legend>{{ t("flipSpeed") }}</legend>
                <label
                  v-for="(name, index) in flipSpeedOptions"
                  :key="name"
                  class="classic-radio"
                >
                  <input
                    v-model.number="panelState.appearance.flipSpeed"
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

              <label class="check-line">
                <input
                  v-model="panelState.appearance.weather"
                  type="checkbox"
                />
                <span>{{ t("showWeather") }}</span>
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
            <form class="panel-form" @submit.prevent>
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
              @submit.prevent
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

              <div class="button-row">
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
            <div class="upcoming-alarm-preview" role="status">
              <span aria-hidden="true">◷</span>
              <strong>{{ upcomingAlarmText }}</strong>
            </div>

            <div class="alarm-grid">
              <form
                v-for="(alarm, alarmIndex) in panelState.alarms"
                :key="alarmIndex"
                class="alarm-card"
                @submit.prevent
              >
                <div class="alarm-heading">
                  <strong>{{ t("alarm") }} {{ alarmIndex + 1 }}</strong>
                  <label class="switch-label">
                    <input v-model="alarm.enabled" type="checkbox" />
                    <span>{{ alarm.enabled ? t("on") : t("off") }}</span>
                  </label>
                </div>

                <label class="field">
                  <span>{{ t("alarmLabel") }}</span>
                  <input
                    v-model.trim="alarm.label"
                    type="text"
                    maxlength="24"
                    :placeholder="`${t('alarm')} ${alarmIndex + 1}`"
                  />
                </label>

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

                <label class="check-line alarm-option">
                  <input v-model="alarm.oneTime" type="checkbox" />
                  <span>{{ t("oneTimeAlarm") }}</span>
                </label>

                <fieldset
                  class="weekdays"
                  :disabled="alarm.oneTime"
                >
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

                <div class="alarm-options">
                  <label class="check-line">
                    <input
                      v-model="alarm.gradualVolume"
                      type="checkbox"
                    />
                    <span>{{ t("gradualVolume") }}</span>
                  </label>
                  <label class="check-line">
                    <input v-model="alarm.sunrise" type="checkbox" />
                    <span>{{ t("sunriseScreen") }}</span>
                  </label>
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
            <form class="panel-form" @submit.prevent>
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
            <form class="panel-form" @submit.prevent>
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

            </form>
          </MacWindow>

          <MacWindow
            v-if="activeApp === 'backup'"
            id="backup"
            ref="activeWindowRef"
            :title="activeAppTitle"
            closable
            :close-label="
              t('closeWindow', { title: activeAppTitle })
            "
            @close="closeActiveApp()"
          >
            <section class="panel-form backup-panel">
              <div class="backup-disk-stack" aria-hidden="true">
                <span></span>
                <span></span>
              </div>

              <div>
                <h2>{{ t("backupTitle") }}</h2>
                <p class="help-text">{{ t("backupDescription") }}</p>
                <ul class="backup-contents">
                  <li>{{ t("backupIncludesSettings") }}</li>
                  <li>{{ t("backupIncludesSounds") }}</li>
                  <li>{{ t("backupIncludesFloppies") }}</li>
                  <li>{{ t("backupIncludesRom") }}</li>
                  <li>{{ t("backupExcludesPassword") }}</li>
                </ul>
              </div>

              <section class="backup-action">
                <h2>{{ t("exportBackup") }}</h2>
                <p class="help-text">{{ t("exportBackupHelp") }}</p>
                <MacButton
                  default-action
                  :disabled="!!busy"
                  @click="downloadConfigurationBackup"
                >
                  {{
                    busy === "configuration-export"
                      ? t("preparingBackup")
                      : t("downloadBackup")
                  }}
                </MacButton>
              </section>

              <section class="backup-action">
                <h2>{{ t("restoreBackup") }}</h2>
                <p class="help-text">{{ t("restoreBackupHelp") }}</p>
                <MacButton
                  secondary
                  :disabled="!!busy"
                  @click="backupFileInput?.click()"
                >
                  {{ t("chooseBackup") }}
                </MacButton>
                <input
                  ref="backupFileInput"
                  class="visually-hidden"
                  type="file"
                  accept=".zip,application/zip"
                  @change="chooseConfigurationBackup"
                />
              </section>
            </section>
          </MacWindow>

          <MacWindow
            v-if="activeApp === 'update'"
            id="update"
            ref="activeWindowRef"
            :title="activeAppTitle"
            closable
            wide
            :close-label="
              t('closeWindow', { title: activeAppTitle })
            "
            @close="closeActiveApp()"
          >
            <section class="panel-form update-panel">
              <div class="update-computer" aria-hidden="true">
                <span>1.0</span>
              </div>

              <dl class="location-summary update-summary">
                <div>
                  <dt>{{ t("installedFirmware") }}</dt>
                  <dd>{{ panelState.update.currentVersion }}</dd>
                </div>
                <div>
                  <dt>{{ t("installedAssets") }}</dt>
                  <dd>{{ panelState.update.assetVersion }}</dd>
                </div>
                <div>
                  <dt>{{ t("latestRelease") }}</dt>
                  <dd>
                    {{ panelState.update.latestVersion || t("notChecked") }}
                  </dd>
                </div>
                <div>
                  <dt>{{ t("changedAssets") }}</dt>
                  <dd>{{ panelState.update.changedAssets }}</dd>
                </div>
              </dl>

              <div
                class="update-status"
                :class="{
                  error: panelState.update.stage === 'error',
                  ready: panelState.update.rebootRequired,
                }"
                role="status"
                aria-live="polite"
              >
                <strong>{{ t(`updateStage_${panelState.update.stage}`) }}</strong>
                <span>{{ panelState.update.message }}</span>
              </div>

              <div
                v-if="panelState.update.busy"
                class="update-progress"
                role="progressbar"
                :aria-valuenow="panelState.update.progress"
                aria-valuemin="0"
                aria-valuemax="100"
              >
                <span
                  :style="{ width: `${panelState.update.progress}%` }"
                ></span>
              </div>

              <p
                v-if="panelState.update.releaseNotes"
                class="update-release-notes"
              >
                {{ panelState.update.releaseNotes }}
              </p>

              <div class="button-row update-actions">
                <MacButton
                  secondary
                  :disabled="!!busy || panelState.update.busy"
                  @click="checkForUpdates"
                >
                  {{ t("checkNow") }}
                </MacButton>
                <MacButton
                  v-if="panelState.update.available"
                  default-action
                  :disabled="!!busy || panelState.update.busy"
                  @click="installUpdate"
                >
                  {{ t("installUpdate") }}
                </MacButton>
                <MacButton
                  v-if="panelState.update.available"
                  secondary
                  :disabled="!!busy"
                  @click="dismissUpdate('later')"
                >
                  {{ t("later") }}
                </MacButton>
                <MacButton
                  v-if="panelState.update.available"
                  secondary
                  :disabled="!!busy"
                  @click="dismissUpdate('ignore')"
                >
                  {{ t("ignoreVersion") }}
                </MacButton>
                <MacButton
                  v-if="panelState.update.rebootRequired"
                  default-action
                  :disabled="!!busy"
                  @click="rebootForUpdate"
                >
                  {{ t("rebootNow") }}
                </MacButton>
              </div>

              <section class="firmware-upload">
                <h2>{{ t("manualFirmware") }}</h2>
                <p class="help-text">{{ t("manualFirmwareWarning") }}</p>
                <MacButton
                  secondary
                  :disabled="!!busy || panelState.update.busy"
                  @click="firmwareFileInput?.click()"
                >
                  {{ t("chooseFirmware") }}
                </MacButton>
                <input
                  ref="firmwareFileInput"
                  class="visually-hidden"
                  type="file"
                  accept=".bin,application/octet-stream"
                  @change="chooseFirmware"
                />
              </section>
            </section>
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
            <form class="sound-manager" @submit.prevent>
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

              <section class="sound-library" :aria-label="t('soundLibrary')">
                <h2>{{ t("soundLibrary") }}</h2>

                <section class="sound-browser">
                  <div class="sound-browser-heading">
                    <h3>{{ t("installedSounds") }}</h3>
                    <span>{{
                      t("soundStorage", {
                        count: downloadedSounds.length,
                        used: formatSoundSize(downloadedSoundBytes),
                        free: formatSoundSize(panelState.storage?.free),
                      })
                    }}</span>
                  </div>
                  <ul v-if="downloadedSounds.length" class="sound-file-list">
                    <li
                      v-for="sound in downloadedSounds"
                      :key="sound.path"
                      class="sound-file-row"
                    >
                      <div class="sound-file-icon" aria-hidden="true">♫</div>
                      <div class="sound-file-details">
                        <strong>{{ sound.name }}</strong>
                        <span>
                          {{ formatSoundSize(sound.size) }}
                          <template v-if="sound.inUse">
                            · {{ t("inUse") }}
                          </template>
                        </span>
                      </div>
                      <div class="sound-file-actions">
                        <MacButton
                          secondary
                          :aria-label="
                            t('previewNamedSound', { name: sound.name })
                          "
                          :disabled="!!busy"
                          @click="previewSound(sound.path, 60)"
                        >
                          ▶
                        </MacButton>
                        <MacButton
                          danger
                          :aria-label="
                            t('removeNamedSound', { name: sound.name })
                          "
                          :title="
                            sound.inUse
                                ? t('inUseCannotRemove')
                                : t('removeSound')
                          "
                          :disabled="!!busy || sound.inUse"
                          @click="requestSoundDeletion(sound)"
                        >
                          {{ t("remove") }}
                        </MacButton>
                      </div>
                    </li>
                  </ul>
                  <p v-else class="empty-sound-list">
                    {{ t("noSoundsInstalled") }}
                  </p>
                </section>

                <div
                  class="sound-drop-zone"
                  :class="{ dragging: soundDragActive }"
                  role="button"
                  tabindex="0"
                  @click="soundFileInput?.click()"
                  @keydown.enter.prevent="soundFileInput?.click()"
                  @keydown.space.prevent="soundFileInput?.click()"
                  @dragenter.prevent="soundDragActive = true"
                  @dragover.prevent="soundDragActive = true"
                  @dragleave.prevent="soundDragActive = false"
                  @drop.prevent="dropSoundFile"
                >
                  <span class="sound-drop-icon" aria-hidden="true">♫</span>
                  <strong>{{ t("dropMp3") }}</strong>
                  <span>{{ t("orChooseFile") }}</span>
                  <small>{{ t("mp3Limit") }}</small>
                </div>
                <input
                  ref="soundFileInput"
                  class="visually-hidden"
                  type="file"
                  accept=".mp3,audio/mpeg"
                  @change="chooseSoundFile"
                />

                <div class="sound-source-grid">
                  <section class="sound-source-box">
                    <h3>{{ t("importFromUrl") }}</h3>
                    <p>{{ t("importUrlHelp") }}</p>
                    <div class="sound-import-line">
                      <input
                        v-model="soundImportUrl"
                        type="url"
                        inputmode="url"
                        :placeholder="t('mp3UrlPlaceholder')"
                        :aria-label="t('mp3UrlPlaceholder')"
                        @keydown.enter.prevent="importSound"
                      />
                      <MacButton
                        default-action
                        :disabled="!!busy || !soundImportUrl.trim()"
                        @click="importSound"
                      >
                        {{ t("importSound") }}
                      </MacButton>
                    </div>
                  </section>

                  <section class="sound-source-box">
                    <h3>{{ t("browseMyInstants") }}</h3>
                    <p>{{ t("myInstantsHelp") }}</p>
                    <div class="sound-import-line">
                      <input
                        v-model="myInstantsQuery"
                        type="search"
                        :placeholder="t('searchSounds')"
                        :aria-label="t('searchSounds')"
                        @keydown.enter.prevent="searchMyInstants"
                      />
                      <MacButton
                        secondary
                        :disabled="
                          !!busy || myInstantsQuery.trim().length < 2
                        "
                        @click="searchMyInstants"
                      >
                        {{
                          busy === "myinstants-search"
                            ? t("searching")
                            : t("search")
                        }}
                      </MacButton>
                    </div>

                    <div
                      v-if="myInstantsSearched"
                      class="myinstants-browser"
                      aria-live="polite"
                    >
                      <div class="myinstants-browser-heading">
                        <strong>{{ t("myInstantsResults") }}</strong>
                        <span>
                          {{
                            t("resultCount", {
                              count: myInstantsResults.length,
                            })
                          }}
                        </span>
                      </div>
                      <ul
                        v-if="myInstantsResults.length"
                        class="myinstants-result-list"
                      >
                        <li
                          v-for="result in myInstantsResults"
                          :key="result.mp3Url"
                          class="myinstants-result-row"
                        >
                          <span>{{ result.name }}</span>
                          <div class="sound-file-actions">
                            <MacButton
                              secondary
                              :aria-label="
                                t('previewNamedSound', {
                                  name: result.name,
                                })
                              "
                              :disabled="!!busy"
                              @click="previewMyInstants(result)"
                            >
                              ▶
                            </MacButton>
                            <MacButton
                              default-action
                              :disabled="!!busy"
                              @click="importMyInstantsResult(result)"
                            >
                              {{ t("addSound") }}
                            </MacButton>
                          </div>
                        </li>
                      </ul>
                      <p v-else class="empty-sound-list">
                        {{ t("noMyInstantsResults") }}
                      </p>
                    </div>
                  </section>
                </div>

              </section>
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
        v-if="restoreConfirmation"
        class="dialog-shade"
        @click.self="cancelConfigurationRestore"
      >
        <section
          class="classic-confirm"
          role="alertdialog"
          aria-modal="true"
          aria-labelledby="restore-backup-title"
          aria-describedby="restore-backup-message"
        >
          <div class="confirm-icon" aria-hidden="true">!</div>
          <div>
            <h2 id="restore-backup-title">{{ t("restoreConfirmTitle") }}</h2>
            <p id="restore-backup-message">
              {{
                t("restoreConfirmMessage", {
                  name: pendingBackupFile?.name || "",
                })
              }}
            </p>
            <p>{{ t("restoreConfirmNetwork") }}</p>
          </div>
          <div class="confirm-actions">
            <MacButton
              secondary
              :disabled="!!busy"
              @click="cancelConfigurationRestore"
            >
              {{ t("cancel") }}
            </MacButton>
            <MacButton
              danger
              default-action
              :disabled="!!busy"
              @click="confirmConfigurationRestore"
            >
              {{
                busy === "configuration-import"
                  ? t("restoringBackup")
                  : t("restoreBackup")
              }}
            </MacButton>
          </div>
        </section>
      </div>
    </Transition>

    <Transition name="dialog">
      <div
        v-if="deleteSoundTarget"
        class="dialog-shade"
        @click.self="deleteSoundTarget = null"
      >
        <section
          class="classic-confirm"
          role="alertdialog"
          aria-modal="true"
          aria-labelledby="delete-sound-title"
          aria-describedby="delete-sound-message"
        >
          <div class="confirm-icon" aria-hidden="true">?</div>
          <div>
            <h2 id="delete-sound-title">{{ t("removeSoundTitle") }}</h2>
            <p id="delete-sound-message">
              {{
                t("removeSoundMessage", {
                  name: deleteSoundTarget.name,
                })
              }}
            </p>
          </div>
          <div class="confirm-actions">
            <MacButton
              secondary
              :disabled="!!busy"
              @click="deleteSoundTarget = null"
            >
              {{ t("cancel") }}
            </MacButton>
            <MacButton
              danger
              default-action
              :disabled="!!busy"
              @click="confirmSoundDeletion"
            >
              {{ t("remove") }}
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
