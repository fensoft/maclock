import { readFile } from "node:fs/promises";
import { faceEditorTranslations } from "../src/i18n.js";

const source = await readFile(new URL("../src/face_editor/FaceEditor.vue", import.meta.url), "utf8");
const keys = [...source.matchAll(/t\(["'](faceEditor\w+)["']/g)].map((match) => match[1]);
const missing = [...new Set(keys)].filter((key) => !faceEditorTranslations[key]);
const incomplete = Object.entries(faceEditorTranslations)
  .filter(([, translations]) => !Array.isArray(translations) || translations.length !== 5 || translations.some((text) => typeof text !== "string" || !text.trim()))
  .map(([key]) => key);

if (missing.length || incomplete.length) {
  if (missing.length) console.error(`Missing Face Editor translations: ${missing.join(", ")}`);
  if (incomplete.length) console.error(`Incomplete Face Editor translations: ${incomplete.join(", ")}`);
  process.exit(1);
}

console.log(`Face Editor i18n coverage passed (${keys.length} referenced keys, ${Object.keys(faceEditorTranslations).length} localized keys).`);
