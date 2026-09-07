import { readdir, readFile, stat } from "node:fs/promises";
import { join, relative } from "node:path";
import { fileURLToPath } from "node:url";

const root = fileURLToPath(new URL("../data/", import.meta.url));
const dynamicAssets = {
  "{weather_asset}.png": ["sunny.png", "cloudy.png", "rainy.png"],
  "macos8_{weather_asset}.png": ["macos8_sunny.png", "macos8_cloudy.png", "macos8_rainy.png"],
  "plugin_{i2c}.png": ["plugin_0x18.png", "plugin_0x38.png", "plugin_0x40.png", "plugin_0x47.png", "plugin_0x50.png", "plugin_0x68.png"],
};
const faceCatalogAssets = new Set(["icon.png"]);
const optionalProjectAssets = new Set(["plugin.png"]);

async function entries(directory) {
  return (await readdir(directory, { withFileTypes: true })).filter((entry) => !entry.name.startsWith("."));
}

async function auditProject(directory, configName) {
  const config = JSON.parse(await readFile(join(directory, configName), "utf8"));
  const expected = new Set([configName]);
  for (const object of config.objects || []) {
    if (object.type !== "image") continue;
    const template = object.template || object.source || "";
    for (const asset of dynamicAssets[template] || [template]) {
      if (!asset || asset.includes("{") || asset.includes("/")) continue;
      expected.add(asset);
    }
  }

  const missing = [];
  for (const asset of expected) {
    try { await stat(join(directory, asset)); }
    catch { missing.push(asset); }
  }
  const files = [];
  for (const entry of await entries(directory)) {
    if (entry.isDirectory()) continue;
    files.push(entry.name);
  }
  const unexpected = files.filter((file) => !expected.has(file) &&
    !(configName === "clockface.json" && faceCatalogAssets.has(file)) &&
    !optionalProjectAssets.has(file));
  return { missing, unexpected, files };
}

async function walk(directory) {
  const result = [];
  for (const entry of await entries(directory)) {
    const path = join(directory, entry.name);
    if (entry.isDirectory()) result.push(...await walk(path));
    else result.push(path);
  }
  return result;
}

let failed = false;
for (const [collection, configName] of [["clockface", "clockface.json"], ["loading", "loading.json"]]) {
  const directory = join(root, collection);
  for (const entry of await entries(directory)) {
    if (!entry.isDirectory()) continue;
    const projectDirectory = join(directory, entry.name);
    const result = await auditProject(projectDirectory, configName);
    for (const asset of result.missing) {
      console.error(`Missing ${collection}/${entry.name}/${asset}`);
      failed = true;
    }
    for (const asset of result.unexpected) {
      console.error(`Unexpected ${collection}/${entry.name}/${asset}`);
      failed = true;
    }
  }
}

const files = await walk(root);
const sizes = await Promise.all(files.map(async (file) => ({ file, bytes: (await stat(file)).size })));
sizes.sort((a, b) => b.bytes - a.bytes);
const total = sizes.reduce((sum, entry) => sum + entry.bytes, 0);
console.log(`LittleFS assets: ${(total / 1024).toFixed(1)} KiB across ${sizes.length} files`);
for (const entry of sizes.slice(0, 10))
  console.log(`${(entry.bytes / 1024).toFixed(1).padStart(7)} KiB  ${relative(root, entry.file)}`);

if (failed) process.exit(1);
