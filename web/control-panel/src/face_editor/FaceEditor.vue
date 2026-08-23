<script setup>
import { computed, nextTick, onBeforeUnmount, onMounted, reactive, ref, watch } from "vue";
import { clockFaceAssetUrl, fetchClockFaceFonts, fetchClockFaceGlyph, fetchClockFaces, listClockFaceAssets, loadClockFace, saveClockFace, selectClockFace, uploadClockFaceAsset } from "../api";

const W = 304;
const H = 224;
const canvas = ref(null);
const preview = ref(null);
const menuBar = ref(null);
const upload = ref(null);
const archiveUpload = ref(null);
const openMenu = ref(null);
const selected = ref(-1);
const status = ref("");
const newObjectType = ref("rectangle");
const selectedPlaceholder = ref("time");
const previewLanguage = ref("en");
const savedFaces = ref([]);
const savedFaceOptions = ref([]);
const selectedFace = ref("compact_digital");
const LAST_FACE_KEY = "maclock.face-editor.last-face";
const lvglFonts = ref([]);
const tick = ref(0);
const previewScale = ref(2);
const drag = ref(null);
const images = new Map();
const imageUrls = new Map();
const localImageTemplates = new Map();
const glyphs = new Map();
const pendingGlyphs = new Set();
const project = ref({
  format: "maclock-clock-face", version: 1, name: "Untitled Face",
  width: W, height: H, background: "#ffffff", random_interval_seconds: 60, translations: {}, objects: [],
});
const values = reactive({
  time_min: "10:42", time_seconds: "10:42:37", hour: "10", hour12: "10", hour_tens: "1", hour_ones: "0", minute: "42", minute_tens: "4", minute_ones: "2", second: "37", second_tens: "3", second_ones: "7", meridiem: "AM",
  date: "09/08/2026", date_iso: "2026-08-09", weekday: "Sunday", weekday_short: "Sun", day: "09", month: "08", month_name: "August", month_short: "Aug", year: "2026", face_name: "Compact Digital",
  internal_temp: "21.5", external_temp: "18.2", external_min: "12.1", external_max: "23.8", temperature_unit: "°C", pressure: "1013", humidity: "48", weather: "Sunny", weather_asset: "sunny", city: "Paris", wifi_ssid: "Mac Host Network", wifi_rssi: "-42", alarm_next: "07:30", alarm_label: "Wake up", timer_remaining: "24:59",
  rtc_available: true, weather_available: true, wifi_available: true, external_sensor_available: true, timer_active: false, floppy_inserted: true, show_time_seconds: false,
});
const objectTypes = ["rectangle", "circle", "line", "text", "image", "flip", "odometer", "odometer_background", "flip_background", "colon"];
const alignments = ["left", "center", "right"];
const object = computed(() => project.value.objects[selected.value] || null);
const availabilityValues = computed(() => Object.keys(values).filter((key) => typeof values[key] === "boolean" && key !== "floppy_inserted"));
const placeholders = computed(() => Object.keys(values));
const translationEntries = computed(() => Object.entries(project.value.translations || {}));
const placeholderOptions = computed(() => [...placeholders.value.map((key) => ({ key, label: `{${key}} (${values[key]})` })), ...translationEntries.value.map(([key, translations]) => ({ key: `tr.${key}`, label: `{tr.${key}} (${translations?.[previewLanguage.value] || translations?.en || ""})` }))]);
const visibleRules = computed({
  get: () => {
    const expression = object.value?.visible_if?.trim();
    if (!expression) return [];
    return expression.split(/\s+and\s+/).map((term) => {
      const plain = term.trim().match(/^(!?)(\w+)$/);
      if (plain) return { key: plain[2], operator: plain[1] ? "!=" : "==", value: "true" };
      const match = term.trim().match(/^(\w+)\s*(==|!=)\s*(.+)$/);
      return match ? { key: match[1], operator: match[2], value: match[3].replace(/^["']|["']$/g, "") } : { key: "", operator: "==", value: "" };
    }).filter((rule) => rule.key);
  },
  set: (rules) => {
    if (!object.value) return;
    object.value.visible_if = rules.filter((rule) => rule.key).map((rule) => `${rule.key} ${rule.operator} ${typeof values[rule.key] === "boolean" ? rule.value : JSON.stringify(rule.value)}`).join(" and ");
    render();
  },
});
const randomLines = computed({
  get: () => object.value?.random?.join("\n") || "",
  set: (value) => { if (object.value) object.value.random = value.split("\n").map((line) => line.trim()).filter(Boolean); },
});
const objectText = computed({
  get: () => object.value ? JSON.stringify(object.value, null, 2) : "",
  set: (value) => { try { project.value.objects[selected.value] = JSON.parse(value); render(); } catch { /* editing incomplete JSON */ } },
});

function makeObject(type) {
  const n = project.value.objects.length + 1;
  const base = { id: `${type}_${n}`, type, x: 32, y: 32, width: 100, height: 40, stroke: "#000000", fill: "transparent", stroke_width: 1, border_radius: 0, visible_if: "" };
  if (type === "line") Object.assign(base, { angle: "", max: 60 });
  if (type === "flip" || type === "odometer") Object.assign(base, { template: "{hour}:{minute}:{second}", font_family: "lv_font_chicago_48", font_size: 48, align: "center" });
  if (type === "text") Object.assign(base, { template: "{time_min}", font_family: "lv_font_chicago_8", font_size: 8, align: "left", random: [] });
  project.value.objects.push(base); selected.value = project.value.objects.length - 1; render();
}
function addSelectedObject() {
  if (newObjectType.value === "image") upload.value?.click();
  else makeObject(newObjectType.value);
}
function removeObject() { if (object.value) { project.value.objects.splice(selected.value, 1); selected.value = Math.min(selected.value, project.value.objects.length - 1); render(); } }
function duplicateObject() { if (!object.value) return; const copy = structuredClone(object.value); copy.id += "_copy"; copy.x += 6; copy.y += 6; project.value.objects.splice(selected.value + 1, 0, copy); selected.value++; render(); }
function moveObject(direction) { if (!object.value) return; const next = selected.value + direction; if (next < 0 || next >= project.value.objects.length) return; const [item] = project.value.objects.splice(selected.value, 1); project.value.objects.splice(next, 0, item); selected.value = next; render(); }
function toggleObjectFlag(key) { if (!object.value) return; object.value.editor ||= {}; object.value.editor[key] = !object.value.editor[key]; render(); }
function translation(key) { const entry = project.value.translations?.[key]; return entry?.[previewLanguage.value] ?? entry?.en ?? Object.values(entry || {}).find(Boolean) ?? `{tr.${key}}`; }
function format(template = "") { return template.replace(/\{([\w.]+)\}/g, (_, key) => key.startsWith("tr.") ? translation(key.slice(3)) : (values[key] ?? `{${key}}`)); }
function valueLabel(key) { return key.replace(/_/g, " ").replace(/\b\w/g, (letter) => letter.toUpperCase()); }
function isVisible(expression = "") {
  if (!expression.trim()) return true;
  return expression.split(/\s+and\s+/).every((term) => {
    const match = term.trim().match(/^(!?)(\w+)(?:\s*(==|!=)\s*(.+))?$/); if (!match) return false;
    const [, not, key, op, raw] = match; const current = values[key];
    if (!op) return not ? !current : !!current;
    const wanted = raw.replace(/["']/g, "").trim(); const equal = String(current) === wanted;
    return op === "==" ? equal : !equal;
  });
}
function randomText(item) {
  if (!item.random?.length) return item.template || "";
  const interval = Math.max(1, Number(project.value.random_interval_seconds) || 60);
  const bucket = Math.floor(tick.value / interval);
  return item.random[bucket % item.random.length];
}
function requestGlyph(font, code) {
  const key = `${font}:${code}`; if (glyphs.has(key) || pendingGlyphs.has(key)) return glyphs.get(key);
  pendingGlyphs.add(key); fetchClockFaceGlyph(font, code).then((glyph) => { if (glyph) glyphs.set(key, glyph); }).catch(() => {}).finally(() => { pendingGlyphs.delete(key); render(); }); return null;
}
function pixelText(ctx, item) {
  const font = item.font_family || "lv_font_chicago_8"; const lines = format(randomText(item)).split("{newline}");
  const fallback = Number(item.font_size) || 8;
  const rgb = color(item.stroke || "#000");
  lines.forEach((line, lineIndex) => {
    const run = [...line].map((character) => ({ glyph: requestGlyph(font, Math.min(0xFF, character.codePointAt(0))) }));
    const width = run.reduce((total, entry) => total + (entry.glyph ? entry.glyph.advance : fallback), 0);
    let x = Number(item.x) || 0; if (item.align === "center") x += ((Number(item.width) || 0) - width) / 2; if (item.align === "right") x += (Number(item.width) || 0) - width;
    for (const entry of run) {
      const glyph = entry.glyph; if (!glyph) { x += fallback; continue; }
      const top = Math.round((Number(item.y) || 0) + lineIndex * glyph.lineHeight + glyph.lineHeight - glyph.baseLine - glyph.height - glyph.offsetY);
      for (let y = 0; y < glyph.height; y += 1) for (let px = 0; px < glyph.width; px += 1) if (glyph.pixels[y * glyph.width + px] === "1") { ctx.fillStyle = `rgb(${rgb[0]} ${rgb[1]} ${rgb[2]})`; ctx.fillRect(Math.floor(x + glyph.offsetX + px), top + y, 1, 1); }
      x += glyph.advance;
    }
  });
}
function objectBounds(item) {
  const x = Number(item.x) || 0, y = Number(item.y) || 0;
  const w = Number(item.width) || 0, h = Number(item.height) || 0;
  if (item.type === "line" && item.angle && Number(item.max) > 0) {
    const value = Number(format(String(item.angle)));
    if (Number.isFinite(value)) {
      const radians = value / Number(item.max) * Math.PI * 2;
      const length = Math.abs(h || w);
      const endX = x + Math.round(Math.sin(radians) * length);
      const endY = y - Math.round(Math.cos(radians) * length);
      return { x: Math.min(x, endX), y: Math.min(y, endY), w: Math.abs(endX - x), h: Math.abs(endY - y) };
    }
  }
  if (item.type !== "text") return { x, y, w, h };

  const font = item.font_family || "lv_font_chicago_8";
  const lines = format(randomText(item)).split("{newline}");
  const fallback = Number(item.font_size) || 8;
  let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
  lines.forEach((line, lineIndex) => {
    const run = [...line].map((character) => requestGlyph(font, Math.min(0xFF, character.codePointAt(0))));
    const textWidth = run.reduce((total, glyph) => total + (glyph ? glyph.advance : fallback), 0);
    let cursor = x;
    if (item.align === "center") cursor += (w - textWidth) / 2;
    if (item.align === "right") cursor += w - textWidth;
    for (const glyph of run) {
      if (!glyph) { cursor += fallback; continue; }
      const top = Math.round(y + lineIndex * glyph.lineHeight + glyph.lineHeight - glyph.baseLine - glyph.height - glyph.offsetY);
      minX = Math.min(minX, Math.floor(cursor + glyph.offsetX));
      minY = Math.min(minY, top);
      maxX = Math.max(maxX, Math.floor(cursor + glyph.offsetX) + glyph.width);
      maxY = Math.max(maxY, top + glyph.height);
      cursor += glyph.advance;
    }
  });
  return Number.isFinite(minX) ? { x: minX, y: minY, w: maxX - minX, h: maxY - minY } : { x, y, w, h: fallback };
}
function resolveImageUrl(item) {
  const source = format(item.template || item.source || "");
  if (!source) return "";
  if (/^(data:|https?:)/.test(source)) return source;
  const asset = source.split("/").pop();
  return selectedFace.value && asset ? clockFaceAssetUrl(selectedFace.value, asset) : source;
}
function ensureImage(item) {
  const url = resolveImageUrl(item);
  const template = format(item.template || item.source || "");
  if (images.has(item.id) && localImageTemplates.get(item.id) === template)
    return;
  if (!url || imageUrls.get(item.id) === url) return;
  imageUrls.set(item.id, url);
  loadImage(url).then((image) => { if (imageUrls.get(item.id) === url) { images.set(item.id, image); item.ratio = image.naturalWidth / image.naturalHeight; render(); } }).catch(() => { if (imageUrls.get(item.id) === url) images.delete(item.id); });
}
function color(input) { const probe = document.createElement("canvas").getContext("2d"); probe.fillStyle = input; const hex = probe.fillStyle; const bits = hex.match(/[\da-f]{2}/gi); return bits ? bits.map((v) => parseInt(v, 16)) : [0, 0, 0]; }
function render() {
  const el = canvas.value; if (!el) return; const ctx = el.getContext("2d");
  ctx.imageSmoothingEnabled = false; ctx.fillStyle = project.value.background || "#fff"; ctx.fillRect(0, 0, W, H);
  project.value.objects.forEach((item) => {
    if (item.editor?.hidden || !isVisible(item.visible_if)) return;
    const x = Number(item.x) || 0, y = Number(item.y) || 0, w = Number(item.width) || 0, h = Number(item.height) || 0;
    ctx.lineWidth = Number(item.stroke_width) || 1; ctx.strokeStyle = item.stroke || "#000"; ctx.fillStyle = item.fill || "transparent";
    if (item.type === "rectangle") { if (Number(item.border_radius) > 0) { ctx.beginPath(); ctx.roundRect(x, y, w, h, Number(item.border_radius)); if (item.fill && item.fill !== "transparent") ctx.fill(); ctx.stroke(); } else { if (item.fill && item.fill !== "transparent") ctx.fillRect(x, y, w, h); ctx.strokeRect(x + 0.5, y + 0.5, Math.max(0, w - 1), Math.max(0, h - 1)); } }
    else if (item.type === "circle") { ctx.beginPath(); ctx.ellipse(x + w / 2, y + h / 2, Math.abs(w / 2), Math.abs(h / 2), 0, 0, Math.PI * 2); if (item.fill && item.fill !== "transparent") ctx.fill(); ctx.stroke(); }
    else if (item.type === "odometer_background") { ctx.beginPath(); ctx.roundRect(x, y, w, h, Number(item.border_radius) || 6); if (item.fill && item.fill !== "transparent") ctx.fill(); ctx.stroke(); }
    else if (item.type === "flip_background") { if (item.fill && item.fill !== "transparent") ctx.fillRect(x, y, w, h); ctx.strokeRect(x + 0.5, y + 0.5, Math.max(0, w - 1), Math.max(0, h - 1)); }
    else if (item.type === "line") { const value = Number(format(String(item.angle || ""))); const rotated = item.angle && Number.isFinite(value) && Number(item.max) > 0; const radians = rotated ? value / Number(item.max) * Math.PI * 2 : 0; const length = Math.abs(h || w); const endX = rotated ? x + Math.round(Math.sin(radians) * length) : x + w; const endY = rotated ? y - Math.round(Math.cos(radians) * length) : y + h; ctx.beginPath(); ctx.moveTo(x + 0.5, y + 0.5); ctx.lineTo(endX + 0.5, endY + 0.5); ctx.stroke(); }
    else if (item.type === "text") pixelText(ctx, item);
    else if (item.type === "flip") { const text = format(randomText(item)); const half = Math.floor((h - 6) / 2); ctx.fillStyle = "#55544e"; ctx.beginPath(); ctx.roundRect(x, y, w, h, 6); ctx.fill(); ctx.strokeStyle = "#77766e"; ctx.stroke(); ctx.fillStyle = "#1d1d1d"; ctx.fillRect(x + 2, y + 2, w - 4, half); ctx.fillStyle = "#101010"; ctx.fillRect(x + 2, y + 4 + half, w - 4, half); ctx.fillStyle = "#b1afa4"; ctx.beginPath(); ctx.roundRect(x + 1, y + Math.floor(h / 2) - 3, 4, 6, 2); ctx.fill(); ctx.beginPath(); ctx.roundRect(x + w - 5, y + Math.floor(h / 2) - 3, 4, 6, 2); ctx.fill(); pixelText(ctx, { ...item, y: y + Math.floor((h - (Number(item.font_size) || 48)) / 2) - 4, template: text, random: [], stroke: "#ffffff" }); }
    else if (item.type === "flip" || item.type === "odometer") { const fallback = item.type === "flip" ? "#181818" : "#080808"; const fill = item.fill && item.fill !== "transparent" ? item.fill : fallback; const text = format(randomText(item)); const cellWidth = text.length ? w / text.length : w; const textY = y + Math.floor((h - (Number(item.font_size) || 48)) / 2) - 6 + (item.type === "odometer" ? 2 : 0); ctx.fillStyle = fill; if (item.type === "odometer" || Number(item.border_radius) > 0) { ctx.beginPath(); ctx.roundRect(x, y, w, h, Number(item.border_radius) || (item.type === "odometer" ? 6 : 0)); ctx.fill(); if (item.type !== "odometer" || text.length > 1) ctx.stroke(); } else { ctx.fillRect(x, y, w, h); ctx.strokeRect(x + 0.5, y + 0.5, Math.max(0, w - 1), Math.max(0, h - 1)); } if (item.type === "flip") { ctx.beginPath(); ctx.moveTo(x, y + h / 2 + 0.5); ctx.lineTo(x + w, y + h / 2 + 0.5); ctx.stroke(); } [...text].forEach((character, index) => { let textX = x + index * cellWidth, textWidth = cellWidth; let clipped = false; if (item.type === "odometer" && /\d/.test(character)) { const windowWidth = cellWidth - 2; const windowX = textX + (cellWidth - windowWidth) / 2; ctx.beginPath(); ctx.roundRect(windowX, y, windowWidth, h, 2); ctx.stroke(); ctx.save(); ctx.beginPath(); ctx.roundRect(windowX, y, windowWidth, h, 2); ctx.clip(); textX = windowX; textWidth = windowWidth; clipped = true; } pixelText(ctx, { ...item, x: textX, y: textY, width: textWidth, template: character, random: [], stroke: "#ffffff" }); if (clipped) ctx.restore(); }); }
    else if (item.type === "colon") { if (!item.blink || tick.value % 2) { ctx.fillStyle = item.stroke || "#ffffff"; [h / 3 - 2, h * 2 / 3 - 2].forEach((dotY) => { ctx.beginPath(); ctx.arc(x + w / 2, y + dotY + 2, 2, 0, Math.PI * 2); ctx.fill(); }); } }
    else if (item.type === "image") { ensureImage(item); if (images.has(item.id)) ctx.drawImage(images.get(item.id), x, y, w, h); }
  });
  if (object.value) { const bounds = objectBounds(object.value); ctx.save(); ctx.setLineDash([2, 2]); ctx.strokeStyle = "#147ef5"; ctx.strokeRect(bounds.x - 2, bounds.y - 2, bounds.w + 4, bounds.h + 4); ctx.restore(); }
}
function pointer(event) { const rect = canvas.value.getBoundingClientRect(); return { x: Math.floor((event.clientX - rect.left) * W / rect.width), y: Math.floor((event.clientY - rect.top) * H / rect.height) }; }
function beginDrag(event) { if (!object.value || object.value.editor?.locked) return; const p = pointer(event), item = object.value; if (p.x < item.x || p.x > item.x + item.width || p.y < item.y || p.y > item.y + item.height) return; drag.value = { p, x: Number(item.x), y: Number(item.y) }; canvas.value.setPointerCapture(event.pointerId); }
function moveDrag(event) { if (!drag.value || !object.value) return; const p = pointer(event); object.value.x = Math.round(drag.value.x + p.x - drag.value.p.x); object.value.y = Math.round(drag.value.y + p.y - drag.value.p.y); render(); }
function endDrag() { drag.value = null; render(); }
async function addImage(event) {
  const file = event.target.files?.[0]; if (!file) return;
  const data = await fileToDataUrl(file); const image = await loadImage(data); const ratio = image.naturalWidth / image.naturalHeight;
  const scale = Math.min(1, 240 / image.naturalWidth, 160 / image.naturalHeight);
  const item = { id: `image_${project.value.objects.length + 1}`, type: "image", template: file.name.replace(/\.[^.]+$/, "") + ".png", x: 20, y: 20, width: Math.round(image.naturalWidth * scale), height: Math.round(image.naturalHeight * scale), keep_ratio: true, visible_if: "" };
  item.assetName = item.template; item.ratio = ratio; images.set(item.id, image); imageUrls.set(item.id, data); localImageTemplates.set(item.id, item.template); project.value.objects.push(item); selected.value = project.value.objects.length - 1; event.target.value = ""; render();
}
function resizeImage(axis) { const item = object.value; if (item?.type === "image" && item.keep_ratio && item.ratio) { if (axis === "height") item.width = Math.round(Number(item.height) * item.ratio); else item.height = Math.round(Number(item.width) / item.ratio); } render(); }
function insertPlaceholder() { if (!object.value) return; object.value.template = `${object.value.template || ""}{${selectedPlaceholder.value}}`; render(); }
function insertImagePlaceholder() { if (!object.value) return; object.value.template = `${object.value.template || object.value.source || ""}{${selectedPlaceholder.value}}`; render(); }
function setPreviewWeather(asset) { values.weather_asset = asset; values.weather = asset.replace(/\b\w/g, (letter) => letter.toUpperCase()); render(); }
function addTranslation() { const key = window.prompt("Translation key", "label"); const normalized = key?.trim().replace(/[^a-zA-Z0-9_-]+/g, "_"); if (!normalized) return; project.value.translations ||= {}; project.value.translations[normalized] ||= { en: normalized, fr: normalized, es: normalized, de: normalized, it: normalized }; }
function removeTranslation(key) { delete project.value.translations?.[key]; }
function addVisibleRule() { visibleRules.value = [...visibleRules.value, { key: "weather_available", operator: "==", value: "true" }]; }
function updateVisibleRule(index, field, value) { const rules = visibleRules.value; rules[index][field] = value; if (field === "key") rules[index].value = typeof values[value] === "boolean" ? "true" : String(values[value] ?? ""); visibleRules.value = rules; }
function removeVisibleRule(index) { const rules = visibleRules.value; rules.splice(index, 1); visibleRules.value = rules; }
function selectFont() { const font = lvglFonts.value.find((entry) => entry.id === object.value?.font_family); if (font) object.value.font_size = font.size; render(); }
function fileToDataUrl(file) { return new Promise((resolve, reject) => { const r = new FileReader(); r.onload = () => resolve(r.result); r.onerror = reject; r.readAsDataURL(file); }); }
function loadImage(source) { return new Promise((resolve, reject) => { const image = new Image(); image.onload = () => resolve(image); image.onerror = reject; image.src = source; }); }
function projectJson() { return JSON.stringify({ ...project.value, objects: project.value.objects.map(({ assetName, ratio, source, editor, ...item }) => item.type === "image" ? { ...item, template: item.template || source || "" } : item) }, null, 2); }
function download(name, blob) { const link = document.createElement("a"); link.href = URL.createObjectURL(blob); link.download = name; link.click(); setTimeout(() => URL.revokeObjectURL(link.href), 1000); }
function crc32(bytes) { let crc = 0xFFFFFFFF; for (const byte of bytes) { crc ^= byte; for (let bit = 0; bit < 8; bit += 1) crc = (crc >>> 1) ^ (0xEDB88320 & -(crc & 1)); } return (crc ^ 0xFFFFFFFF) >>> 0; }
function zipEntries(entries) { const encoder = new TextEncoder(); const chunks = []; const central = []; let offset = 0; const u16 = (value) => Uint8Array.of(value & 255, (value >>> 8) & 255); const u32 = (value) => Uint8Array.of(value & 255, (value >>> 8) & 255, (value >>> 16) & 255, (value >>> 24) & 255); const join = (parts) => { const size = parts.reduce((sum, part) => sum + part.length, 0); const data = new Uint8Array(size); let at = 0; parts.forEach((part) => { data.set(part, at); at += part.length; }); return data; }; entries.forEach(({ name, data }) => { const nameBytes = encoder.encode(name); const crc = crc32(data); const local = join([u32(0x04034B50), u16(20), u16(0), u16(0), u16(0), u16(0), u32(crc), u32(data.length), u32(data.length), u16(nameBytes.length), u16(0), nameBytes, data]); chunks.push(local); central.push(join([u32(0x02014B50), u16(20), u16(20), u16(0), u16(0), u16(0), u16(0), u32(crc), u32(data.length), u32(data.length), u16(nameBytes.length), u16(0), u16(0), u16(0), u16(0), u32(0), u32(offset), nameBytes])); offset += local.length; }); const centralData = join(central); return new Blob([...chunks, centralData, join([u32(0x06054B50), u16(0), u16(0), u16(entries.length), u16(entries.length), u32(centralData.length), u32(offset), u16(0)])], { type: "application/zip" }); }
function unzipEntries(buffer) { const data = new Uint8Array(buffer); const view = new DataView(buffer); const decoder = new TextDecoder(); const files = new Map(); let offset = 0; while (offset + 4 <= data.length && view.getUint32(offset, true) === 0x04034B50) { const method = view.getUint16(offset + 8, true); const size = view.getUint32(offset + 18, true); const nameLength = view.getUint16(offset + 26, true); const extraLength = view.getUint16(offset + 28, true); if (method !== 0) throw new Error("Compressed ZIP files are not supported"); const start = offset + 30 + nameLength + extraLength; files.set(decoder.decode(data.slice(offset + 30, offset + 30 + nameLength)), data.slice(start, start + size)); offset = start + size; } return files; }
async function exportJson() { try { await navigator.clipboard.writeText(projectJson()); status.value = "JSON copied to clipboard."; } catch { status.value = "Clipboard export is unavailable."; } }
async function exportArchive() { if (!selectedFace.value) { status.value = "Open a saved clock face before exporting its archive."; return; } try { const assets = (await listClockFaceAssets(selectedFace.value)).assets || []; const entries = [{ name: "clockface.json", data: new TextEncoder().encode(projectJson()) }]; for (const asset of assets) { const response = await fetch(clockFaceAssetUrl(selectedFace.value, asset)); if (!response.ok) throw new Error(); entries.push({ name: asset, data: new Uint8Array(await response.arrayBuffer()) }); } download(`${selectedFace.value}.zip`, zipEntries(entries)); status.value = `Exported ${entries.length} archive files.`; } catch { status.value = "Clock-face archive export failed."; } }
async function importArchive(event) { const file = event.target.files?.[0]; if (!file) return; try { const entries = unzipEntries(await file.arrayBuffer()); const json = entries.get("clockface.json"); if (!json) throw new Error(); const parsed = JSON.parse(new TextDecoder().decode(json)); if (parsed.width !== W || parsed.height !== H) throw new Error(); const name = window.prompt("Import archive as", safeFaceName(parsed.name)); if (name === null) return; const face = safeFaceName(name); await saveClockFace(face, JSON.stringify(parsed)); for (const [asset, data] of entries) if (asset !== "clockface.json" && asset.endsWith(".png")) await uploadClockFaceAsset(face, asset, new Blob([data], { type: "image/png" })); await refreshSavedFaces(); selectedFace.value = face; await openFromMaclock(); status.value = `Imported ${entries.size} archive files.`; } catch { status.value = "That is not a supported Maclock face ZIP archive."; } finally { event.target.value = ""; } }
async function refreshSavedFaces() { try { const result = await fetchClockFaces(); savedFaces.value = result.faces || []; savedFaceOptions.value = await Promise.all(savedFaces.value.map(async (id) => { const face = await loadClockFace(id); return { id, name: face.name || id }; })); if (!savedFaces.value.includes(selectedFace.value) && savedFaces.value.length) selectedFace.value = savedFaces.value[0]; } catch { status.value = "Could not list clock faces from Maclock."; } }
async function openFromMaclock() {
  try {
    const loaded = await loadClockFace(selectedFace.value); project.value = loaded; images.clear(); imageUrls.clear(); localImageTemplates.clear();
    for (const item of project.value.objects.filter((entry) => entry.type === "image")) {
      item.template ||= item.source || "";
      const asset = String(format(item.template)).split("/").pop(); if (!asset) continue;
      const url = clockFaceAssetUrl(selectedFace.value, asset); const image = await loadImage(url); images.set(item.id, image); imageUrls.set(item.id, url); item.assetName = asset; item.ratio = image.naturalWidth / image.naturalHeight;
    }
    selected.value = project.value.objects.length ? 0 : -1; localStorage.setItem(LAST_FACE_KEY, selectedFace.value); status.value = `Loaded ${selectedFace.value} from Maclock.`; render(); return true;
  } catch { status.value = "Could not load this clock face from Maclock."; return false; }
}
function safeFaceName(name) { return String(name || "").toLowerCase().replace(/[^a-z0-9_-]+/g, "_").replace(/^_+|_+$/g, "") || "clock_face"; }
function newProject() {
  project.value = {
    format: "maclock-clock-face", version: 1, name: "Untitled Face",
    width: W, height: H, background: "#ffffff", random_interval_seconds: 60, translations: {}, objects: [],
  };
  selected.value = -1;
  selectedFace.value = "";
  localStorage.removeItem(LAST_FACE_KEY);
  images.clear(); imageUrls.clear(); localImageTemplates.clear();
  status.value = "New clock face. Add layers, then Save As…";
  render();
}
async function saveToMaclock(name) {
  const face = safeFaceName(typeof name === "string" ? name : (selectedFace.value || project.value.name));
  try {
    for (const item of project.value.objects.filter((entry) => entry.type === "image")) {
      const image = images.get(item.id); if (!image) continue; const out = document.createElement("canvas"); out.width = image.naturalWidth; out.height = image.naturalHeight; out.getContext("2d").drawImage(image, 0, 0); const blob = await new Promise((resolve) => out.toBlob(resolve, "image/png")); await uploadClockFaceAsset(face, item.assetName || `${item.id}.png`, blob);
    }
    localImageTemplates.clear(); await saveClockFace(face, projectJson()); await refreshSavedFaces(); selectedFace.value = face; localStorage.setItem(LAST_FACE_KEY, face); status.value = `Saved /clockface/${face}/clockface.json`; render();
  } catch { status.value = "Maclock could not save this clock face."; }
}
async function saveAsToMaclock() {
  const name = window.prompt("Save clock face as", safeFaceName(project.value.name));
  if (name === null) return;
  project.value.name = name.trim() || project.value.name;
  await saveToMaclock(name);
}
async function useOnMaclock() {
  if (!selectedFace.value) return;
  try { await selectClockFace(selectedFace.value); status.value = `Using ${selectedFace.value} on Maclock.`; } catch { status.value = "Maclock could not select this clock face."; }
}
async function openAndUseFace(face) {
  selectedFace.value = face.id;
  if (await openFromMaclock())
    await useOnMaclock();
}
async function importJson() { try { const parsed = JSON.parse(await navigator.clipboard.readText()); if (parsed.width !== W || parsed.height !== H) throw new Error(); project.value = parsed; selected.value = parsed.objects.length ? 0 : -1; status.value = "JSON imported from clipboard. Existing face assets were preserved."; render(); } catch { status.value = "Clipboard does not contain a 304×224 Maclock face JSON project."; } }
let timer = 0;
let previewResizeObserver = null;
function updatePreviewScale() {
  const availableWidth = window.innerWidth <= 900
    ? Math.max(W, window.innerWidth - 32)
    : Math.max(W, window.innerWidth - 160 - 280 - 64);
  const availableHeight = Math.max(H, window.innerHeight - 210);
  previewScale.value = Math.max(1, Math.min(4, Math.floor(Math.min(availableWidth / W, availableHeight / H))));
}
function nudge(event) { if (!object.value || /INPUT|TEXTAREA/.test(document.activeElement?.tagName || "")) return; const amount = event.shiftKey ? 10 : 1; const moves = { ArrowLeft: [-amount, 0], ArrowRight: [amount, 0], ArrowUp: [0, -amount], ArrowDown: [0, amount] }; const move = moves[event.key]; if (!move) return; event.preventDefault(); object.value.x = Number(object.value.x) + move[0]; object.value.y = Number(object.value.y) + move[1]; render(); }
function toggleMenu(menu) { openMenu.value = openMenu.value === menu ? null : menu; }
function closeMenus() { openMenu.value = null; }
function runMenuCommand(command) { closeMenus(); return command(); }
function closeMenuOnOutsideClick(event) { if (!menuBar.value?.contains(event.target)) closeMenus(); }
function closeMenuOnEscape(event) { if (event.key === "Escape") closeMenus(); }
onMounted(async () => { render(); updatePreviewScale(); window.addEventListener("resize", updatePreviewScale); previewResizeObserver = new ResizeObserver(updatePreviewScale); if (preview.value) previewResizeObserver.observe(preview.value); const fontResult = await fetchClockFaceFonts(); lvglFonts.value = fontResult.fonts || []; await refreshSavedFaces(); const lastFace = localStorage.getItem(LAST_FACE_KEY); if (lastFace && savedFaces.value.includes(lastFace)) selectedFace.value = lastFace; if (selectedFace.value) await openFromMaclock(); window.addEventListener("keydown", nudge); timer = window.setInterval(() => { tick.value = Math.floor(Date.now() / 1000); render(); }, 1000); });
onMounted(() => { window.addEventListener("pointerdown", closeMenuOnOutsideClick); window.addEventListener("keydown", closeMenuOnEscape); });
onBeforeUnmount(() => { window.clearInterval(timer); window.removeEventListener("keydown", nudge); window.removeEventListener("keydown", closeMenuOnEscape); window.removeEventListener("pointerdown", closeMenuOnOutsideClick); window.removeEventListener("resize", updatePreviewScale); previewResizeObserver?.disconnect(); });
watch(project, () => nextTick(render), { deep: true });
</script>

<template>
  <section class="face-editor">
    <Teleport to="#face-editor-menu-host">
    <nav ref="menuBar" class="face-editor__menu-bar" aria-label="Face editor menus">
      <div class="face-editor__menu"><button type="button" aria-haspopup="menu" :aria-expanded="openMenu === 'file'" @click="toggleMenu('file')">File</button><div v-if="openMenu === 'file'" class="face-editor__menu-popover" role="menu"><button type="button" role="menuitem" @click="runMenuCommand(newProject)">New</button><button type="button" role="menuitem" @click="runMenuCommand(() => saveToMaclock())">Save</button><button type="button" role="menuitem" @click="runMenuCommand(saveAsToMaclock)">Save As...</button><hr><button type="button" role="menuitem" @click="runMenuCommand(() => archiveUpload?.click())">Import archive</button><button type="button" role="menuitem" @click="runMenuCommand(exportArchive)" :disabled="!selectedFace">Export archive</button><button type="button" role="menuitem" @click="runMenuCommand(importJson)">Import JSON</button><button type="button" role="menuitem" @click="runMenuCommand(exportJson)">Export JSON</button></div></div>
      <div class="face-editor__menu"><button type="button" aria-haspopup="menu" :aria-expanded="openMenu === 'edit'" @click="toggleMenu('edit')">Edit</button><div v-if="openMenu === 'edit'" class="face-editor__menu-popover" role="menu"><label class="face-editor__menu-label">New object<select v-model="newObjectType"><option v-for="type in objectTypes" :key="type" :value="type">{{ type }}</option></select></label><button type="button" role="menuitem" @click="runMenuCommand(addSelectedObject)">Add object</button><hr><button type="button" role="menuitem" @click="runMenuCommand(() => moveObject(-1))" :disabled="!object || selected === 0">Bring back</button><button type="button" role="menuitem" @click="runMenuCommand(() => moveObject(1))" :disabled="!object || selected === project.objects.length - 1">Bring forward</button><button type="button" role="menuitem" @click="runMenuCommand(() => toggleObjectFlag('locked'))" :disabled="!object">{{ object?.editor?.locked ? 'Unlock' : 'Lock' }}</button><button type="button" role="menuitem" @click="runMenuCommand(() => toggleObjectFlag('hidden'))" :disabled="!object">{{ object?.editor?.hidden ? 'Show' : 'Hide' }}</button><button type="button" role="menuitem" @click="runMenuCommand(duplicateObject)" :disabled="!object">Duplicate</button><button type="button" role="menuitem" @click="runMenuCommand(removeObject)" :disabled="!object">Delete</button></div></div>
      <div class="face-editor__menu"><button type="button" aria-haspopup="menu" :aria-expanded="openMenu === 'object'" @click="toggleMenu('object')">Object</button><div v-if="openMenu === 'object'" class="face-editor__menu-popover face-editor__object-list" role="menu"><button v-for="(item, index) in project.objects" :key="item.id" type="button" role="menuitem" :class="{ selected: selected === index }" @click="runMenuCommand(() => { selected = index; render(); })">{{ item.id }} <small>{{ item.type }}</small></button><span v-if="!project.objects.length" class="face-editor__menu-empty">No objects</span></div></div>
      <div class="face-editor__menu"><button type="button" aria-haspopup="menu" :aria-expanded="openMenu === 'view'" @click="toggleMenu('view')">View</button><div v-if="openMenu === 'view'" class="face-editor__menu-popover" role="menu"><fieldset class="face-editor__availability"><legend>Preview availability</legend><label v-for="key in availabilityValues" :key="key"><input v-model="values[key]" type="checkbox" @change="render"> {{ valueLabel(key) }}</label><label><input v-model="values.floppy_inserted" type="checkbox" @change="render"> Floppy inserted</label><label>Weather <select :value="values.weather_asset" @change="setPreviewWeather($event.target.value)"><option value="sunny">Sunny</option><option value="cloudy">Cloudy</option><option value="rainy">Rainy</option></select></label><label>Language <select v-model="previewLanguage"><option value="en">English</option><option value="fr">French</option><option value="es">Spanish</option><option value="de">Deutsch</option><option value="it">Italian</option></select></label></fieldset></div></div>
      <div class="face-editor__menu"><button type="button" aria-haspopup="menu" :aria-expanded="openMenu === 'face'" @click="toggleMenu('face')">Face</button><div v-if="openMenu === 'face'" class="face-editor__menu-popover" role="menu"><button v-for="face in savedFaceOptions" :key="face.id" type="button" role="menuitem" :class="{ selected: selectedFace === face.id }" @click="runMenuCommand(() => openAndUseFace(face))">{{ face.name }}</button><span v-if="!savedFaceOptions.length" class="face-editor__menu-empty">No saved clock faces</span></div></div>
      <span class="face-editor__status" role="status">{{ status }}</span>
    </nav>
    </Teleport>
    <input ref="upload" class="visually-hidden" type="file" accept="image/png,image/jpeg,image/svg+xml" @change="addImage"><input ref="archiveUpload" class="visually-hidden" type="file" accept="application/zip,.zip" @change="importArchive">
    <div class="face-editor__layout">
      <div ref="preview" class="face-editor__preview"><canvas ref="canvas" width="304" height="224" :style="{ width: `${W * previewScale}px`, height: `${H * previewScale}px` }" @pointerdown="beginDrag" @pointermove="moveDrag" @pointerup="endDrag" @pointercancel="endDrag" /></div>
      <aside class="face-editor__properties">
        <details><summary>Face</summary><div class="face-editor__section"><label>Name <input v-model="project.name"></label><label>Id <input :value="selectedFace" readonly></label><label>Background <input v-model="project.background"></label><label>Random every (seconds) <input v-model.number="project.random_interval_seconds" type="number" min="1"></label></div></details>
        <details><summary>Translations</summary><div class="face-editor__section"><p class="face-editor__hint">Use <code>{tr.key}</code> in text layers. English is the fallback.</p><div v-for="([key, entry]) in translationEntries" :key="key" class="face-editor__translation"><strong>{{ key }}</strong><button type="button" title="Remove translation" @click="removeTranslation(key)">−</button><label>EN <input v-model="entry.en"></label><label>FR <input v-model="entry.fr"></label><label>ES <input v-model="entry.es"></label><label>DE <input v-model="entry.de"></label><label>IT <input v-model="entry.it"></label></div><button type="button" @click="addTranslation">+ Add translation</button></div></details>
        <template v-if="object">
            <details><summary>Object · {{ object.id }}</summary><div class="face-editor__section"><label>Id <input v-model="object.id"></label><label>Type <select v-model="object.type"><option v-for="type in objectTypes" :key="type">{{ type }}</option></select></label><label>X <input v-model.number="object.x" type="number"></label><label>Y <input v-model.number="object.y" type="number"></label><label>Width <input v-model.number="object.width" type="number" @change="resizeImage('width')"></label><label>Height <input v-model.number="object.height" type="number" @change="resizeImage('height')"></label><label v-if="object.type === 'line'">Angle <input v-model="object.angle" placeholder="{hour}" @input="render"></label><label v-if="object.type === 'line'">Max <input v-model.number="object.max" type="number" min="1" @input="render"></label><label v-if="object.type === 'colon'"><input v-model="object.blink" type="checkbox"> Blink</label></div></details>
            <details><summary>Appearance</summary><div class="face-editor__section"><label>Stroke <input v-model="object.stroke" placeholder="#000000"></label><label>Fill <select v-model="object.fill"><option value="transparent">Transparent</option><option value="#ffffff">White</option><option value="#000000">Black</option></select></label><label>Stroke width <input v-model.number="object.stroke_width" type="number" min="0"></label><label v-if="!['line', 'text', 'image', 'colon'].includes(object.type)">Border radius <input v-model.number="object.border_radius" type="number" min="0"></label></div></details>
          <details><summary>Visible when…</summary><div class="face-editor__section"><p class="face-editor__hint">All conditions must match. No condition means always visible.</p><div v-for="(rule, index) in visibleRules" :key="index" class="face-editor__rule"><select :value="rule.key" @change="updateVisibleRule(index, 'key', $event.target.value)"><option v-for="placeholder in placeholders" :key="placeholder" :value="placeholder">{{ valueLabel(placeholder) }}</option></select><select :value="rule.operator" @change="updateVisibleRule(index, 'operator', $event.target.value)"><option value="==">is</option><option value="!=">is not</option></select><select v-if="typeof values[rule.key] === 'boolean'" :value="rule.value" @change="updateVisibleRule(index, 'value', $event.target.value)"><option value="true">available / yes</option><option value="false">unavailable / no</option></select><input v-else :value="rule.value" @input="updateVisibleRule(index, 'value', $event.target.value)" :placeholder="String(values[rule.key] ?? '')"><button type="button" title="Remove condition" @click="removeVisibleRule(index)">−</button></div><button type="button" @click="addVisibleRule">+ Add condition</button></div></details>
          <details v-if="['text', 'flip', 'odometer'].includes(object.type)"><summary>Text</summary><div class="face-editor__section"><label>Template <input v-model="object.template" placeholder="{time_min}"></label><div class="face-editor__placeholder"><select v-model="selectedPlaceholder"><option v-for="placeholder in placeholderOptions" :key="placeholder.key" :value="placeholder.key">{{ placeholder.label }}</option></select><button type="button" @click="insertPlaceholder">Insert</button></div><label>LVGL pixel font <select v-model="object.font_family" @change="selectFont"><option v-for="font in lvglFonts" :key="font.id" :value="font.id">{{ font.id }} · {{ font.size }} px{{ font.digitsOnly ? ' · digits' : '' }}</option></select></label><label>Font size <input v-model.number="object.font_size" type="number" min="1" @change="render"></label><label>Alignment <select v-model="object.align"><option v-for="alignment in alignments" :key="alignment">{{ alignment }}</option></select></label><label v-if="object.type === 'text'">Random text (one per line) <textarea v-model="randomLines" rows="5" spellcheck="false"></textarea></label></div></details>
          <template v-if="object.type === 'image'">
            <details><summary>Image</summary><div class="face-editor__section"><label>PNG template <input v-model="object.template" :placeholder="object.source || 'image.png'"></label><div class="face-editor__placeholder"><select v-model="selectedPlaceholder"><option v-for="placeholder in placeholderOptions" :key="placeholder.key" :value="placeholder.key">{{ placeholder.label }}</option></select><button type="button" @click="insertImagePlaceholder">Insert</button></div><label>Asset filename <input v-model="object.assetName"></label><label><input v-model="object.keep_ratio" type="checkbox"> Keep image ratio</label></div></details>
          </template>
          <details><summary>Advanced JSON</summary><div class="face-editor__section"><label>Advanced object JSON <textarea v-model="objectText" rows="8" spellcheck="false"></textarea></label></div></details>
        </template>
      </aside>
    </div>
  </section>
</template>

<style scoped>
.face-editor { width: 100%; font-family: monospace; }
.face-editor__layout { display: grid; grid-template-columns: max-content minmax(280px, 1fr); gap: 16px; align-items: start; }
.face-editor aside { display: grid; gap: 6px; }
.face-editor button, .face-editor input, .face-editor textarea { border: 1px solid #000; border-radius: 0; background: #fff; color: #000; font: inherit; }
.face-editor button { padding: 4px 6px; text-align: left; cursor: pointer; }.face-editor button:disabled { color: #777; cursor: default; }.face-editor__layer.selected { color: #fff; background: #000; }.face-editor__layer small { float: right; }
.face-editor__actions, .face-editor__layer-controls { display: grid; gap: 4px; margin: 8px 0; }.face-editor__preview { overflow: auto; padding: 0; border: 2px solid #000; background: #fff; width: fit-content; }.face-editor canvas { display: block; flex: none; background: #fff; image-rendering: pixelated; image-rendering: crisp-edges; touch-action: none; }
.face-editor__properties label { display: grid; gap: 2px; }.face-editor textarea { width: 100%; resize: vertical; }.face-editor__menu-bar { position: relative; display: flex; flex-wrap: wrap; gap: 0; align-items: center; min-height: 0; margin: 0; border: 0; background: transparent; }.face-editor__menu { position: relative; }.face-editor__menu > button { border-width: 0 1px 0 0; padding: 6px 10px; }.face-editor__menu > button[aria-expanded="true"] { color: #fff; background: #000; }.face-editor__menu-popover { position: absolute; z-index: 2; top: 100%; left: -1px; display: grid; min-width: 190px; max-width: min(320px, calc(100vw - 24px)); gap: 4px; padding: 6px; border: 1px solid #000; background: #fff; box-shadow: 3px 3px 0 #000; }.face-editor__menu-popover button { width: 100%; }.face-editor__object-list { max-height: min(420px, calc(100vh - 48px)); overflow-y: auto; }.face-editor__object-list small { float: right; }.face-editor__object-list .selected { color: #fff; background: #000; }.face-editor__menu-popover hr { width: 100%; margin: 2px 0; border: 0; border-top: 1px solid #000; }.face-editor__menu-label { display: grid; gap: 3px; font-weight: bold; }.face-editor__menu-label select { min-width: 0; }.face-editor__status { align-self: center; min-width: 0; padding: 4px 8px; font-size: 0.9em; overflow-wrap: anywhere; }
.face-editor details { border: 1px solid #000; background: #fff; }.face-editor summary { padding: 5px 7px; cursor: pointer; font-weight: bold; }.face-editor__section { display: grid; gap: 6px; padding: 7px; border-top: 1px solid #000; }.face-editor__availability { display: grid; gap: 4px; margin: 0; padding: 7px; border: 0; }.face-editor__availability label { display: block; }.face-editor__availability input { margin: 0 4px 0 0; }
.face-editor__placeholder { display: grid; grid-template-columns: minmax(0, 1fr) auto; gap: 4px; }.face-editor__placeholder button { text-align: center; }
.face-editor__hint { margin: 0; font-size: 0.9em; }.face-editor__rule { display: grid; grid-template-columns: minmax(0, 1fr) auto minmax(0, 1fr) auto; gap: 4px; align-items: center; }
.face-editor__translation { display: grid; grid-template-columns: 1fr auto; gap: 5px; padding: 6px; border: 1px solid #000; }.face-editor__translation label { grid-column: span 2; grid-template-columns: max-content minmax(0, 1fr); align-items: center; }.face-editor__translation input { min-width: 0; width: 100%; }
@media (max-width: 900px) { .face-editor__layout { grid-template-columns: 1fr; }.face-editor__preview { order: -1; } }
@media (max-width: 480px) { .face-editor__menu-bar { margin-bottom: 12px; }.face-editor__menu > button { padding: 6px 8px; }.face-editor__menu-popover { position: fixed; top: 44px; left: 12px; right: 12px; max-width: none; }.face-editor__status { width: 100%; border-top: 1px solid #000; } }
</style>
