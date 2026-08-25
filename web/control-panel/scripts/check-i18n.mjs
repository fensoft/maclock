import { readdir, readFile } from "node:fs/promises";
import { fileURLToPath } from "node:url";
import { extname, join } from "node:path";
import { hasTranslation, languageCodes } from "../src/i18n.js";

const sourceDirectory = fileURLToPath(new URL("../src/", import.meta.url));

async function sourceFiles(directory) {
  const entries = await readdir(directory, { withFileTypes: true });
  const files = await Promise.all(entries.map((entry) => {
    const path = join(directory, entry.name);
    return entry.isDirectory()
      ? sourceFiles(path)
      : [".js", ".vue"].includes(extname(entry.name)) ? [path] : [];
  }));
  return files.flat();
}

const files = await sourceFiles(sourceDirectory);
const sources = await Promise.all(files.map(async (path) => [path, await readFile(path, "utf8")]));
const keys = new Set();

for (const [, source] of sources) {
  for (const match of source.matchAll(/\bt\(\s*["']([^"']+)["']/g)) keys.add(match[1]);
  for (const match of source.matchAll(/\btitleKey\s*:\s*["']([^"']+)["']/g)) keys.add(match[1]);
}

const missing = languageCodes.flatMap((language, index) =>
  [...keys]
    .filter((key) => !hasTranslation(index, key))
    .map((key) => `${language}:${key}`),
);

if (missing.length) {
  console.error(`Missing localized strings: ${missing.join(", ")}`);
  process.exit(1);
}

console.log(`i18n coverage passed (${keys.size} static keys across ${files.length} source files and ${languageCodes.length} locales).`);
